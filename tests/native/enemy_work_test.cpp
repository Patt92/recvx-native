#include <cstdlib>
// C02: the shared enemy work at its owner. Component checks against the
// semantics references (eneset.c:170/325/344/797, zonzon1.c:806,
// Motion.c:21) and the production binding in RoomWorld with original room
// data. Positive effects are checked, not only the absence of errors.
#include "world.h"
#include "actor.h"
#include "motion_set.h"
#include "asset_io.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

namespace {
std::vector<std::string> gaps;std::vector<std::uint32_t> order;unsigned clears=0;
RECVX_ENEMY_TABLE* table=nullptr;
void record_gap(const char* name){gaps.push_back(name);}
void clear_offset(){++clears;}
std::uint32_t objects(const RECVX_ENEMY_WORK*){return 3;}
void proc(RECVX_ENEMY_WORK* w){order.push_back(table->enow);check(w->pxb==1.f&&w->axb==7,"pass backup taken after the routine");w->px=5;w->dam[0]=9;w->dam[3]=4;w->comb_timeout=2;w->comb_flg=0x7f;w->flg2|=0x4;}
RECVX_ENEMY_TEMPLATE tmpl(std::uint16_t id,std::uint32_t flg=1){RECVX_ENEMY_TEMPLATE t{};t.flg=flg;t.id=id;t.type=3;t.flr_no=0;t.mdlver=2;t.px=1;t.py=0;t.pz=2;t.ay=-16384;return t;}

void work_contract(){
    RECVX_ENEMY_SERVICES sv{};sv.gap=record_gap;sv.player_clear_offset=clear_offset;sv.obj_num=objects;
    RECVX_ENEMY_TABLE t;recvx_enemy_table_init(&t);table=&t;
    static RECVX_ENEMY_WORK w[130];
    auto a=tmpl(7);auto* first=recvx_enemy_set(&t,&w[0],&a,&w[0],0,&sv);
    check(first==&w[0]&&w[0].idx_ct==0&&t.ewk_n==1,"bhSetEnemy: first slot / ewk_n");
    check(w[0].ay==-16384&&w[0].ax==0&&w[0].mtn_add==65536&&w[0].sx==1.f&&w[0].clp_jno[0]==1&&w[0].clp_jno[1]==-1,"bhSetEnemy: field initialisation");
    auto b=tmpl(45);recvx_enemy_set(&t,&w[1],&b,&w[1],1,&sv);
    // eneset.c:302 overwrites the id-41..48 set after the switch.
    check((w[1].mdflg&0x20)&&(w[1].flg&0x8000)&&w[1].clp_jno[0]==1&&w[1].clp_jno[1]==-1&&w[1].clp_jno[2]==5&&w[1].clp_jno[6]==19,"bhSetEnemy: id>40 flags and clp_jno quirk");
    auto c=tmpl(95);recvx_enemy_set(&t,&w[2],&c,&w[2],2,&sv);
    check((w[2].mdflg&0x120)==0x120&&w[2].clp_jno[3]==17,"bhSetEnemy: id>90 mdflg 0x100");
    // A released slot is reused before the end; ewk_n keeps the gap.
    w[1].flg=0;recvx_enemy_check_work_num(&t);check(t.ewk_n==3,"bhCheckEneWorkNum stops at a gap");
    auto d=tmpl(8);check(recvx_enemy_set(&t,&w[3],&d,&w[3],3,&sv)==&w[3]&&w[3].idx_ct==1&&t.slot[1]==&w[3],"bhSetEnemy does not take the first free slot");
    for(unsigned i=3;i<128;++i){auto f=tmpl(0);check(recvx_enemy_set(&t,&w[i+1],&f,&w[i+1],i+1,&sv)!=nullptr,"table fills early");}
    auto full=tmpl(0);check(!recvx_enemy_set(&t,&w[129],&full,&w[129],129,&sv)&&t.ewk_n==128,"full ene[128] not reported or ewk_n stale");

    // eneset.c:344 order: offset clear before the sp_flg test.
    RECVX_ENEMY_TABLE p;recvx_enemy_table_init(&p);table=&p;static RECVX_ENEMY_WORK v[6];
    for(unsigned i=0;i<6;++i){auto x=tmpl(std::uint16_t(i==3?60:i==4?2:7));recvx_enemy_set(&p,&v[i],&x,&v[i],i,&sv);v[i].px=1;v[i].ax=7;}
    v[1].stflg|=0x1000000;v[2].flg|=0x80000000u;v[5].flg|=0x80;v[5].lkwkp=&v[1];
    RECVX_ENEMY_DISPATCH dsp;std::memset(&dsp,0,sizeof dsp);recvx_enemy_set_call_func(&dsp,7,proc);
    gaps.clear();order.clear();clears=0;
    check(recvx_enemy_control(&p,&dsp,0,&sv)==0&&clears==1&&order.empty(),"sp_flg 0x2 clear still ran routines or skipped the offset clear");
    const auto ran=recvx_enemy_control(&p,&dsp,2,&sv);
    check(ran==2&&order==std::vector<std::uint32_t>({0,2}),"routine order / filter differs");
    check(p.enow==5,"sys->enow not set for every slot");
    check(v[0].px==5&&v[0].dam[0]==0&&v[0].dam[2]==0&&v[0].dam[3]==4&&v[0].comb_timeout==1&&v[0].comb_flg==0x0f&&!(v[0].flg2&4),"bhEne_InitDamage after the routine differs");
    check(p.en_objn==1&&p.en_obj[0]==&v[2],"flg 0x80000000 not collected into en_obj");
    std::set<std::string> named(gaps.begin(),gaps.end());
    check(named.count("bhEne02")&&!named.count("bhSubpl"),"missing routine hidden or subplayer reported as missing");
    check(v[1].pxb==0&&v[5].pxb==0,"filtered works touched");
    // Draw/target filters.
    check(!recvx_enemy_draw_enabled(&v[2],0)&&recvx_enemy_draw_enabled(&v[0],0),"draw filter for en_obj works");
    v[0].mdflg|=0x200;check(!recvx_enemy_draw_enabled(&v[0],0)&&recvx_enemy_draw_enabled(&v[0],0x20),"bhEneRenderSet gate ignores st_flg 0x20");
    v[0].flg|=0x8000;check(!recvx_enemy_targetable(&v[0]),"flg 0x8000 still targetable");
}

MotionClip clip3(bool slow){MotionClip c;c.frames=4;c.flags=slow?0x20:0;c.positions.resize(3);c.angles.resize(3);
    for(unsigned f=0;f<4;++f){c.positions[0].push_back({float(f),10.f+f,0});c.positions[1].push_back({100.f+f,0,0});c.positions[2].push_back({200.f+f,0,0});
        for(unsigned ch=0;ch<3;++ch)c.angles[ch].push_back({std::int32_t(1000*f+ch),std::int32_t(2000*f),std::int32_t(3000*f)});}
    c.attributes={11,12,13,14};return c;}

void motion_contract(){
    const auto clip=clip3(false);std::vector<ModelPose> obj(4);std::vector<std::uint32_t> owp(4,0);
    MotionWork m{};m.frm_no=65536;
    check(set_motion(m,65536,0,&clip,nullptr,obj,owp)==0&&m.frm_no==131072&&m.mtn_attr==12,"bhSetMotion: advance/attribute");
    // Fast: root position from channel 0, angles per channel; samples before advancing.
    check(obj[0].position.x==1&&obj[0].position.y==11&&obj[1].angles[0]==1001&&obj[2].angles[1]==2000,"SetMtnFast pose");
    m.frm_no=3*65536;check(set_motion(m,65536,0,&clip,nullptr,obj,owp)==1&&m.frm_no==0,"forward wrap");
    m.frm_no=0;check(set_motion(m,-65536,0,&clip,nullptr,obj,owp)==-1&&m.frm_no==3*65536,"backward wrap");
    // Ping-pong (mode 0x1): direction flips at the ends.
    m.frm_no=2*65536;m.frm_mode=0;set_motion(m,65536,1,&clip,nullptr,obj,owp);check(m.frm_mode!=0&&m.frm_no==3*65536,"mode 1 flip at end");
    set_motion(m,65536,1,&clip,nullptr,obj,owp);check(m.frm_no==2*65536,"mode 1 runs backwards after the flip");
    // owP flg 0x4 skips a node without consuming a channel (Fast).
    owp={0,4,0,0};obj.assign(4,ModelPose{});m=MotionWork{};set_motion(m,0,0,&clip,nullptr,obj,owp);
    check(obj[1].angles[0]==0&&obj[2].angles[0]==1&&obj[3].angles[0]==2,"owP 0x4 consumed a channel");
    // Slow: fractional key blend.
    const auto slow=clip3(true);obj.assign(4,ModelPose{});owp.assign(4,0);m=MotionWork{};m.frm_no=65536+32768;set_motion(m,0,0,&slow,nullptr,obj,owp);
    check(std::fabs(obj[1].position.x-101.5f)<1e-5f&&obj[0].angles[0]==1500,"SetMtnSlow blend");
    // Hokan: rate 1-hokan_rate/65536 towards the key; counter decrements.
    obj.assign(4,ModelPose{});m=MotionWork{};m.frm_no=65536;m.hokan_count=2;m.hokan_rate=32768;set_motion(m,0,0x100,&clip,nullptr,obj,owp);
    check(m.hokan_count==1&&obj[1].angles[0]==500&&obj[0].position.x==1,"SetMtnFastHokan blend/0x100 root");
    // A datP == NULL motion leaves everything untouched.
    m=MotionWork{};m.frm_no=7;check(set_motion(m,65536,0,nullptr,nullptr,obj,owp)==0&&m.frm_no==7,"empty MN_WORK advanced");
}

void room(const std::filesystem::path& data,const char* name,unsigned id,unsigned expected){
    const auto bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);
    const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},name);
    Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);world.bind_player_collision(events);
    unsigned bound=0;
    for(unsigned i=0;i<64;++i)if(const auto* e=world.state(1,i);e&&e->enemy_bound&&e->identity==id){
        ++bound;check(e->enemy_native&&e->enemy.owner==e&&e->enemy.owner_index==i&&e->enemy.idx_ct>=0,"work not a member of its entity");}
    check(bound==expected,"room enemies not bound in room order");
    world.set_simulation_flags(~0u);
    Scene before=scene;world.append(before);
    for(unsigned pass=0;pass<30;++pass)world.tick();
    Scene after=scene;world.append(after);
    check(after.triangles.size()>before.triangles.size(),"native enemy works are not drawn after their pass");
    for(unsigned i=0;i<64;++i)if(const auto* e=world.state(1,i);e&&e->enemy_native){
        // room.c:447 ruft bhInitMask nur fuer id > 90; sonst bleibt owP frei.
        if(e->identity<=90)for(const auto flg:e->enemy_owp_flg)check(!(flg&4u),"bhInitMask auf einen Gegner mit id <= 90 angewandt");
        check(e->enemy_calculated&&e->enemy.mode0!=0,"routine did not initialise / bhCalcModel did not run");
        check(e->transform.position.x==e->enemy.px&&e->motion==e->enemy.mtn_no&&e->hp==e->enemy.hp,"publish Work -> Entity differs");
        if(id==7&&e->identity==7)check(e->enemy.hp==1&&e->enemy.mtn_no==3&&e->enemy.mode0==1&&(e->enemy.flg&0x68)==0x68,"bhEne07_Init state differs");
        if(id==0&&e->identity==0){NJS_POINT3 probe{e->enemy.px,e->enemy.py+5,e->enemy.pz};check(world.collision_contact(probe,0,1,10).has_value(),"barrel sys->mwalp surface missing from collision");}
    }
    if(id==7){
        // The frame clock is the work's: bhSetMotion advanced it in every pass.
        const auto* e=world.state(1,0);check(e->enemy.frm_no!=0||e->enemy.mtn_add==0,"bhSetMotion did not advance the bat clock");
        // bhEneSetCk through the entity write is taken over before the next pass.
        world.deactivate_enemy(0);world.tick();check(e->enemy.stflg&0x1000000u,"script stflg write not adopted");
        Scene hidden=scene;world.append(hidden);check(hidden.triangles.size()<after.triangles.size(),"inactive work still drawn");
    }
}
// room.c:445-449 ruft bhInitMask fuer jede Raumfigur mit id > 90; face.c:52-60
// setzt dort mlwP->owP[6..14].flg |= 0x4 (ohne mskp face.c:21-25 nur 6..10).
// Motion.c:131 SetMtnNormal ueberspringt genau diese Knoten, sodass die 22
// Kanaele der RM_0000-Raummotionen auf die uebrigen 22 Modellknoten fallen.
void mask_skip(const std::filesystem::path& data){
    const auto bytes=read_asset_file(data/"rooms"/"RM_0000.RDT",64*1024*1024);
    const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0000");
    for(unsigned i=0;i<2;++i){const auto* e=world.state(1,i);
        check(e&&e->enemy_bound&&e->identity>90,"RM_0000 scene actor missing");
        check(e->enemy_owp_flg.size()==27,"RM_0000 scene actor model node count differs");
        unsigned animated=0;for(const auto flg:e->enemy_owp_flg)if(!(flg&4u))++animated;
        check(animated==22,"bhInitMask leaves a channel count the room motion cannot fill");
        for(unsigned n=6;n<=10;++n)check(e->enemy_owp_flg[n]&4u,"bhInitMask did not mark owP[6..14]");
        check(!(e->enemy_owp_flg[5]&4u)&&!(e->enemy_owp_flg[11]&4u),"bhInitMask marked a node outside owP[6..14]");}
}
// RM_0020: das emtp der Identitaet 1 traegt den unteren Koerper bei mtn (8
// Kanaele) und den verknuepften Oberkoerper bei mtn + 0xc8 (10 Kanaele,
// en01_walk_mtn en01.c:1118). Der Oberkoerperwork des Originals schreibt beide
// mit objP/owP += 8 auf denselben Objektbaum (en01.c:2815, :10347-10353); fehlt
// die Zusammenfuehrung, bleiben Arme und Kopf in der Ruhelage (T-Pose), waehrend
// der Zombie laeuft. Vorher/Nachher in diesem Raum gemessen: ohne die
// Zusammenfuehrung bewegten sich 0 der 10 Oberkoerperknoten, mit ihr 8.
void cemetery_upper_body(const std::filesystem::path& data){
    const auto bytes=read_asset_file(data/"rooms"/std::string("RM_0020.RDT"),64*1024*1024);
    const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0020");
    Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);world.bind_player_collision(events);
    world.set_simulation_flags(~0u);
    const WorldEntity* zombie=nullptr;
    for(unsigned i=0;i<64;++i)if(const auto* e=world.state(1,i);e&&e->enemy_bound&&e->identity==1)zombie=e;
    check(zombie!=nullptr,"RM_0020 Zombie fehlt");
    const auto& asset=*zombie->enemy_asset;
    check(asset.bones()==18,"RM_0020 Zombie-Skelett weicht ab");
    const auto rest=asset.bind_pose();
    for(unsigned pass=0;pass<8;++pass)world.tick();
    // en01_flipTree (en01.c:2400) fuer 18 Kanaele: Kanal 8..17 -> Knoten
    // 8,9,10,11,15,16,17,12,13,14.
    static const unsigned upper[10]={8,9,10,11,15,16,17,12,13,14};
    unsigned posed=0;
    for(const auto node:upper){const auto& now=zombie->enemy_obj.at(node);const auto& bind=rest.at(node);
        if(now.position.x!=bind.position.x||now.position.y!=bind.position.y||now.position.z!=bind.position.z||
           now.angles[0]!=bind.angles[0]||now.angles[1]!=bind.angles[1]||now.angles[2]!=bind.angles[2])++posed;}
    // Die Knoten 8 und 10 tragen im Paar (2, 202) dieses Raums reine
    // Nullschluessel; die uebrigen acht kommen aus dem Oberkoerperclip.
    check(posed>=8,"Oberkoerper des Zombies bleibt in der Ruhelage (T-Pose)");
}

// Debug policy must not become a persistent original player death flag.
// Exercise the actual get/set binding, including a fresh destination world.
void debug_player_flags_contract(const std::filesystem::path& data){
    auto player=PlayerAsset::load(data,10);
    Inventory inventory;inventory.hp=160;
    std::uint32_t head_flags=0;
    for(const char* name:{"RM_0080","RM_1000"}){
        const auto bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);
        const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},name);
        MessageDialog dialog;EventVm events(bytes,inventory,dialog);
        world.set_player_asset(&player);world.bind_player_collision(events);
        world.bind_enemy_player(inventory.hp,head_flags);
        const auto original_flags=events.flags[13][0];
        for(bool enabled:{false,true,false}){
            world.set_enemy_grab(enabled);
            RECVX_ENEMY_PLAYER view{};
            check(world.enemy_player_get(view),"debug flags: player binding absent");
            check(view.debug_attack_disabled==!enabled,"debug flags: policy not exposed");
            check(view.flg==original_flags,"debug policy contaminated original flags");
            check(view.hp==160,"debug flags: HP changed during read or room replacement");
            check(world.enemy_player_set(view),"debug flags: player write failed");
            check(inventory.hp==160&&!world.player_dead()&&!world.player_killed(),
                  "debug round trip killed living player");
            check(events.flags[13][0]==original_flags&&events.player_flags==original_flags,
                  "debug round trip persisted synthetic death flag");
        }
        // A real death flag must still propagate, even with attacks disabled.
        RECVX_ENEMY_PLAYER dead{};
        check(world.enemy_player_get(dead),"death flag: player binding absent");
        dead.flg|=2u;
        check(world.enemy_player_set(dead),"death flag: write failed");
        check(world.player_dead()&&(events.player_flags&2u)&&inventory.hp==160,
              "real death flag was hidden by the debug policy");
    }
}


// event.c:352 initializes enemy works before scd0.
// RM_0070 commands 24/25 select mode0=5 and the linked zombie upper motion.
void event_enemy_owner_contract(const std::filesystem::path& data){
    const auto bytes=read_asset_file(data/"rooms/RM_0070.RDT",64*1024*1024);
    const auto scene=decode_scene(bytes);
    auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0070");
    auto player=PlayerAsset::load(data,10);
    Inventory inventory;inventory.hp=160;
    std::uint32_t head_flags=0;
    MessageDialog dialog;EventVm events(bytes,inventory,dialog);
    world.set_player_asset(&player);
    world.bind_player_collision(events);
    world.bind_enemy_player(inventory.hp,head_flags);
    world.set_enemy_ai(true);world.set_enemy_grab(true);
    events.values[0]=0;events.values[1]=7;
    events.binding=[&](const EventCommand& c){
        if(std::getenv("RECVX_EVENT_FLAGS_TRACE")){
            std::cerr<<"EVENT task="<<c.task<<" offset="<<c.offset<<" bytes=";
            for(auto byte:c.bytes)std::cerr<<unsigned(byte)<<' ';
            std::cerr<<'\n';
        }
        return world.bind(c);
    };
    events.task_started=[&](unsigned task){world.start_task(task);};
    events.animation_frame=[&](unsigned kind,unsigned index){return world.frame(kind,index);};
    events.initialize();
    events.tick();
    check(events.errors.empty(),"RM_0070 event setup blocked");
    const unsigned motions[]={1,51,13,5};
    const unsigned upper_motions[]={2,251,213,6};
    for(unsigned i=0;i<4;++i){
        const auto* e=world.state(1,i);
        check(e&&e->enemy_bound,"RM_0070 native enemy missing");
        check(e->enemy.exp0&&e->enemy.exp1,"enemy initialization did not precede event commands");
        std::cout<<"Event owner "<<i<<": mode0="<<unsigned(e->enemy.mode0)
                 <<" native motion="<<e->enemy.mtn_no
                 <<" event motion="<<e->motion<<'\n';
        check(e->enemy.mode0==5,"event command did not select native event dispatch");
        check(e->enemy.mtn_no==motions[i],"event motion did not reach native owner");
        check(e->enemy.ct2==6,"event command lost original hard-transition mode");
        const auto* upper=static_cast<const RECVX_ENEMY_WORK*>(e->enemy.exp1);
        check(upper->mode0==6,"linked upper body retained autonomous dispatch");
        check(upper->mtn_no==upper_motions[i],"linked upper motion uses wrong library offset");
        check(upper->mtn_room==e->enemy.mtn_room,"upper and lower motion libraries differ");
    }
}

}

int main(int argc,char** argv){try{
    _Make_SinTable();
    work_contract();
    motion_contract();
    check(argc==2,"original data directory required");
    const std::filesystem::path data(argv[1]);
    debug_player_flags_contract(data);
    event_enemy_owner_contract(data);
    room(data,"RM_0130",7,5);   // five original ID-7 entries
    room(data,"RM_0080",0,1);   // the room's one barrel next to five zombies
    mask_skip(data);            // RM_0000: Claire/Rodrigo, bhInitMask owP skip
    cemetery_upper_body(data);  // RM_0020: verknuepfter Oberkoerper des Zombies
    std::cout<<"Enemy work contract, bhSetMotion and the RoomWorld binding passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
