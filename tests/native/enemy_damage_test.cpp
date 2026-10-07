#include "world.h"
#include "en01_damage.h"
#include <cstdlib>
#include "asset_io.h"
#include "room.h"
#include "weapon_tables.h"
#include <iostream>
#include <stdexcept>
#include <set>
#include <cmath>
#include <algorithm>
using namespace recvx;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Real en01 zombies of the original room: hits run through bhEne01_DmgCheck/DG/MV04/DD and the original MTNs.
int main(int argc,char** argv){try{_Make_SinTable();
    // HP consumes one rand() call; en01 InitType00 also draws its initial walk variant.
    // Verify both the selected HP and the following random value, without room resources.
    RECVX_EN01_HOST host{};recvx_en01_set_host(&host);
    for(int hard:{0,1})for(unsigned seed=1;seed<=32;++seed){
        std::srand(seed);const unsigned index=unsigned(std::rand())%16;const int dog_next=std::rand();const int zombie_next=std::rand();
        std::srand(seed);RECVX_EN01 zombie{};recvx_en01_init(&zombie,0,0,hard,18,0,0,0,0,0,nullptr,0);
        RECVX_EN01_STATE state{};recvx_en01_state(&zombie,&state);
        check(state.hp==en01_hp_tbl[hard][index],"zombie HP bypasses original rand/table");
        check(std::rand()==zombie_next,"zombie Init changes original random-call count");
        std::srand(seed);RECVX_EN01 dog{};recvx_en04_init(&dog,0,hard,0,0,0,0,0);recvx_en01_state(&dog,&state);
        check(state.hp==en04_hp_tbl[hard][index],"dog HP bypasses original rand/table");
        check(std::rand()==dog_next,"dog Init changes original random-call count");
    }
    std::cout<<"Original HP table selection and random consumption passed (2 enemies, 2 modes, 32 seeds).\n";
    if(argc<2){std::cout<<"no data directory; en01 damage test skipped\n";return 0;}
    const auto data=std::filesystem::u8path(argv[1]);const auto output=argc>2?std::filesystem::u8path(argv[2]):std::filesystem::path();
    AudioMixer audio(data);audio.load_room_sounds(0,2);check(audio.load_player_sounds(0),"Claire CORE bank missing");
    if(argc>3&&std::string(argv[3])=="scan"){ // list enemy identities per original room (barrels are en00)
        for(const auto& entry:std::filesystem::directory_iterator(data/"rooms")){const auto name=entry.path().stem().string();if(entry.path().extension()!=".RDT")continue;
            try{auto room=read_asset_file(entry.path(),64*1024*1024);auto scene=decode_scene(room);SceneSpawn spawn=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;RoomWorld w(room,scene,spawn,m,f,audio,data,{},name);
                std::string row;for(unsigned i=0;i<64;++i)if(const auto* e=w.state(1,i))if(e->identity==0){row+=" #"+std::to_string(i)+"("+std::to_string(int(e->transform.position.x))+","+std::to_string(int(e->transform.position.z))+")";
                    for(unsigned j=0;j<64;++j)if(const auto* z=w.state(1,j))if(z->identity==1){const float d=std::hypot(z->transform.position.x-e->transform.position.x,z->transform.position.z-e->transform.position.z);if(d<40)row+=" zombie#"+std::to_string(j)+"@"+std::to_string(int(d));}}
                if(!row.empty())std::cout<<name<<row<<"\n";}catch(const std::exception&){}}
        return 0;}
    auto bytes=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);auto stage=decode_scene(bytes);SceneSpawn player=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;
    RoomWorld world(bytes,stage,player,motion,frame,audio,data,{},"RM_0020");
    const auto targets=world.combat_targets();check(targets.size()>=2,"original zombies missing");
    const unsigned index=targets[0].index;const auto start=*world.position(1,index);
    auto strike=[&](int weapon,int damage){const auto at=*world.position(1,index);return world.damage(index,weapon,damage,0x10,{at.x,at.y+10,at.z},{0,0,-1});};
    auto log=[&](const char* what){const auto* s=world.state(1,index);std::cout<<what<<" motion="<<s->motion<<" frame="<<(s->frame>>16)<<" hp="<<s->hp<<" dead="<<s->dead<<" pos="<<s->transform.position.x<<","<<s->transform.position.z<<"\n";};
    std::set<unsigned> motions;auto step=[&](unsigned n){for(unsigned i=0;i<n;++i){world.tick();motions.insert(world.state(1,index)->motion);}};
    const unsigned idle=world.state(1,index)->motion;
    // A single pistol hit is the original flinch (DmgCheckType00 act 0): object 8 twists for 8 ticks, no new motion.
    check(strike(3,5),"hit rejected");const int hp_before=world.state(1,index)->hp;step(1);log("after pistol hit");
    check(world.blood_particles()>0,"pistol hit shows no blood (bhEne_SetBlood2 particles)");
    check(world.state(1,index)->hp<hp_before,"hit does not reduce hp");
    check(!world.state(1,index)->node_overrides.empty(),"single hit shows no flinch (bhEne01_SetMtn rot_tbl)");
    step(12);check(!world.state(1,index)->flinch,"flinch never ends");check(world.state(1,index)->motion==idle,"single hit changes the motion");
    // Original combo: three near pistol hits inside the timeout reach the critical points (CombWepTbl crit 25) -> knockback (DG03/DG04).
    step(20);check(strike(3,3),"combo hit 2 rejected");step(6);check(strike(3,3),"combo hit 3 rejected");step(3);log("combo");
    unsigned reaction=world.state(1,index)->motion;check(reaction!=idle,"critical combo does not stagger (no DG motion)");
    step(150);log("after reaction");check(!world.enemy_dead(index),"non-lethal combo kills");
    // Lethal hit: damage motion, then an original death motion; no board-like fall.
    motions.clear();check(strike(3,10000),"lethal hit rejected");
    unsigned frames=0;for(;frames<600&&!world.enemy_dead(index);++frames)step(1);log("death");
    check(world.enemy_dead(index),"zombie never dies");
    const unsigned death=world.state(1,index)->motion;
    step(30);check(world.blood_particles()>0,"corpse leaves no blood pool (bhSetBloodPoolLnk)");
    if(argc>3&&std::string(argv[3])=="blood-only"){
        Scene composed=stage;world.append(composed,{},stage.cameras[0]);
        check(!composed.triangles.empty(),"corpse scene is empty");
        const auto pool_texture=composed.triangles.back().texture;
        unsigned pool_triangles=0;const float expected=world.position(1,index)->y+.11f;
        for(const auto& t:composed.triangles)if(t.textured&&t.texture==pool_texture){++pool_triangles;
            for(const auto& v:t.vertices)check(std::abs(v.position.y-expected)<.0001f,"original corpse pool has wrong height");}
        check(pool_triangles==14,"original corpse has duplicate pools");
        if(!output.empty()){SoftwareRenderer image(640,480);image.draw(composed,stage.cameras[0]);image.save_bmp(output/"corpse-blood.bmp");}
        SceneSpawn reentered=stage.spawns.at(0);std::size_t reload_motion=42;std::int64_t reload_frame=0;
        RoomWorld reload(bytes,stage,reentered,reload_motion,reload_frame,audio,data,{},"RM_0020");reload.tick();
        check(reload.blood_particles()==0,"cemetery reentry adopted old corpse blood pools");
        std::cout<<"RM_0020 original hits/death: one blood-pool strip at Y="<<expected<<"; new room world retains zero old blood effects.\n";return 0;
    }
    std::cout<<"idle="<<idle<<" reaction="<<reaction<<" death="<<death<<" motions=";for(auto m:motions)std::cout<<m<<' ';std::cout<<"\n";
    check(motions.size()>=2,"death shows no animation sequence");
    std::cout<<"enemy voices played:";for(const auto& n:audio.played)if(n.rfind("ARMS",0)!=0)std::cout<<' '<<n;std::cout<<"\nunavailable:";for(const auto& n:audio.unavailable)std::cout<<' '<<n;std::cout<<"\n";
    std::cout<<"active blood effects after death: "<<world.blood_particles()<<"\n";
    std::cout<<"unbound original routines:";for(const auto& name:world.unbound_enemy_routines())std::cout<<"\n  "<<name;std::cout<<"\n";
    {   // Zombie AI (original MVType00/ActionModeCheck/MV00..03/05, foot lock): with the AI on it walks to the player; off, it stands.
        auto room=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);auto scene=decode_scene(room);SceneSpawn me=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;
        for(const bool ai:{false,true}){
            RoomWorld w(room,scene,me,m,f,audio,data,{},"RM_0020");w.set_enemy_ai(ai);const unsigned id=index;w.tick(); /* the first tick settles the spawn against the walls */ const auto zombie=*w.position(1,id);const int yaw=w.state(1,id)->transform.yaw;
            const float k=6.28318530718f/65536.f;me.position={zombie.x-std::sin(yaw*k)*28.f,zombie.y,zombie.z-std::cos(yaw*k)*28.f};
            const float before=std::hypot(me.position.x-zombie.x,me.position.z-zombie.z);std::set<unsigned> seen;
            SceneCamera view=stage.cameras[0];view.hidden_objects.fill(0);view.distance=0;view.perspective=9000;
            {const auto z0=*w.position(1,id);const NJS_POINT3 mid{(z0.x+me.position.x)/2,z0.y+10,(z0.z+me.position.z)/2};view.position={mid.x+30.f,mid.y+6.f,mid.z+34.f};const float dx=mid.x-view.position.x,dy=mid.y-view.position.y,dz=mid.z-view.position.z,k2=65536.f/6.28318530718f;view.angles[0]=int(-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*k2)&65535;view.angles[1]=(32768-int(std::atan2(dx,dz)*k2))&65535;view.angles[2]=0;}
            for(unsigned t=0;t<420;++t){w.tick();seen.insert(w.state(1,id)->motion);
                if(ai&&!output.empty()&&(t==20||t==60||t==120||t==200)){Scene composed=stage;w.append(composed,{},view);SoftwareRenderer image(640,480);image.draw(composed,view);image.save_bmp(output/("walk-t"+std::to_string(t)+".bmp"));}if(getenv("RECVX_DBG")&&(t<4||t%100==0)){const auto q=*w.position(1,id);std::cout<<"  t"<<t<<" pos "<<q.x<<","<<q.z<<" mtn "<<w.state(1,id)->motion<<"\n";}}
            const auto after=*w.position(1,id);const float distance=std::hypot(me.position.x-after.x,me.position.z-after.z);
            std::cout<<"AI "<<(ai?"on":"off")<<": distance "<<before<<" -> "<<distance<<", motions:";for(auto mo:seen)std::cout<<' '<<mo;std::cout<<" mode "<<int(w.state(1,id)->hp>0)<<"\n";
            const float moved=std::hypot(after.x-zombie.x,after.z-zombie.z);if(ai)check(moved>6.f&&distance<before-4.f,"zombie AI does not approach the player");else check(moved<0.5f,"idle zombie moves although the AI is off");
        }
    }
    {   // Zombie grab/bite (EatCheck, NG00, PlyDG00, PlayerLink): default off; with the AI and the grab on the zombie seizes the player.
        auto player_asset=PlayerAsset::load(data);
        auto room=read_asset_file(data/"rooms/RM_0020.RDT",64*1024*1024);auto scene=decode_scene(room);
        for(const unsigned mode:{0u,1u,2u}){ // 0: grab off, 1: grab on and mash, 2: grab on and let it kill the player
            SceneSpawn me=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;
            RoomWorld w(room,scene,me,m,f,audio,data,{},"RM_0020");w.set_player_asset(&player_asset);w.set_enemy_ai(true);w.set_enemy_grab(mode!=0);
            w.tick();const unsigned id=index;const auto z=*w.position(1,id);const int yaw=w.state(1,id)->transform.yaw;const float k=6.28318530718f/65536.f;
            me.position={z.x-std::sin(yaw*k)*20.f,z.y,z.z-std::cos(yaw*k)*20.f};
            const auto voice_begin=audio.played.size();bool voice_pcm=false;
            int hp=mode==2?6:200;bool grabbed=false,released=false;unsigned grabbed_ticks=0;std::set<std::size_t> clips;
            for(unsigned t=0;t<900;++t){w.set_player_hp(hp);w.set_player_lever(mode==1&&w.player_grabbed()?(t&1?0xfu|0x2d00u:0x0u):0u);w.tick();{const bool voice=audio.active(7);const auto pcm=audio.mix();if(voice&&std::any_of(pcm.begin(),pcm.end(),[](auto n){return n!=0;}))voice_pcm=true;}hp-=w.take_blast_damage();if(hp<0)hp=0;
                if(w.player_grabbed()){grabbed=true;++grabbed_ticks;clips.insert(size_t(m));}else if(grabbed)released=true;
                if(!output.empty()&&mode>0&&(t==60||t==70||t==80||t==90||t==100||t==110||t==120||t==130||t==140||t==150||t==160||t==180||t==240)){ // grabbed/bitten/death frames
                    const auto mid=*w.position(1,id);SceneCamera view=scene.cameras[0];view.hidden_objects.fill(0);view.distance=0;view.perspective=9000;
                    const NJS_POINT3 c{me.position.x,me.position.y+10,me.position.z};const float sx=(t/10)%2?36.f:-36.f;view.position={c.x+sx,c.y+4.f,c.z+10.f};
                    const float dx=c.x-view.position.x,dy=c.y-view.position.y,dz=c.z-view.position.z,k2=65536.f/6.28318530718f;view.angles[0]=int(-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*k2)&65535;view.angles[1]=(32768-int(std::atan2(dx,dz)*k2))&65535;view.angles[2]=0;(void)mid;
                    std::vector<BoneMatrix> nodes;Scene body=w.sample_player(player_asset,&nodes);Scene composed=scene;w.append(composed,nodes,view);const auto base=unsigned(composed.textures.size());composed.textures.insert(composed.textures.end(),player_asset.textures().begin(),player_asset.textures().end());append_actor(composed,body,base);
                    SoftwareRenderer image(640,480);image.draw(composed,view);image.save_bmp(output/("grab"+std::to_string(mode)+"-t"+std::to_string(t)+".bmp"));}
                if(w.player_dead())break;}
            std::cout<<"grab mode "<<mode<<": grabbed="<<grabbed<<" ticks="<<grabbed_ticks<<" released="<<released<<" hp="<<hp<<" dead="<<w.player_dead()<<" killed="<<w.player_killed()<<" clips:";for(auto c:clips)std::cout<<' '<<c;std::cout<<"\n";
            if(mode==0)check(!grabbed,"grab happens although it is switched off");
            if(mode!=0){bool routed=false;for(auto i=voice_begin;i<audio.played.size();++i)routed|=audio.played[i].find("/4/0002")!=std::string::npos;check(routed&&voice_pcm,"grab does not play original Claire bite voice PCM");}
            if(mode==1)check(grabbed&&released&&hp<200,"zombie does not grab/release the player");
            if(mode==2)check(w.player_killed()&&w.player_dead(),"fatal bite does not kill the player");
        }
    }
    {   // Explosive barrel (en00): a shot explodes it, the blast (effect 16 after 16 ticks) hurts zombies in range and the wall opens.
        auto room=read_asset_file(data/"rooms/RM_3160.RDT",64*1024*1024);auto scene=decode_scene(room);SceneSpawn spawn=scene.spawns.at(0);std::size_t m=42;std::int64_t f=0;
        RoomWorld w(room,scene,spawn,m,f,audio,data,{},"RM_3160");const unsigned barrel=3,zombie=1;
        check(w.state(1,barrel)&&w.state(1,barrel)->identity==0,"RM_3160 barrel missing");
        bool targeted=false;for(const auto& t:w.combat_targets())targeted|=t.index==barrel&&t.identity==0;check(targeted,"barrel is not a combat target");
        const int zombie_hp=w.state(1,zombie)->hp;const auto at=*w.position(1,barrel);
        check(w.damage(barrel,3,0,0x10,{at.x,at.y+8,at.z},{0,0,-1}),"barrel hit rejected");
        for(unsigned t=0;t<60;++t)w.tick();
        check(w.state(1,barrel)->dead&&w.state(1,barrel)->hidden,"barrel does not explode (bhEne00 case 1)");
        check(w.state(1,zombie)->hp<zombie_hp,"explosion does not hurt the zombie in range (bhCheckBombAtari)");
        std::cout<<"barrel exploded; zombie hp "<<zombie_hp<<" -> "<<w.state(1,zombie)->hp<<" motion "<<w.state(1,zombie)->motion<<"\n";
        if(!output.empty())for(unsigned c=0;c<scene.cameras.size()&&c<4;++c){RoomWorld view(room,scene,spawn,m,f,audio,data,{},"RM_3160");const auto z=*view.position(1,barrel);
            for(unsigned t=0;t<50;++t){if(t==5)view.damage(barrel,3,0,0x10,{z.x,z.y+8,z.z},{0,0,-1});view.tick();
                if(t==4||t==8||t==12||t==20||t==30||t==49){Scene composed=scene;view.append(composed,{},scene.cameras[c]);SoftwareRenderer image(640,480);image.draw(composed,scene.cameras[c]);image.save_bmp(output/("barrel-cam"+std::to_string(c)+"-t"+std::to_string(t)+".bmp"));}}}
    }
    if(!output.empty()){
        // Visual check: a camera looking at the zombie from two sides (host diagnostic camera, same math as the 3rd-person mod).
        for(unsigned side=0;side<2;++side){RoomWorld w(bytes,stage,player,motion,frame,audio,data,{},"RM_0020");
            const auto z=*w.position(1,index);SceneCamera camera=stage.cameras[0];camera.hidden_objects.fill(0);camera.distance=0;camera.position={z.x+(side?-30.f:30.f),z.y+16,z.z+(side?-34.f:34.f)};
            const float dx=z.x-camera.position.x,dy=z.y+10-camera.position.y,dz=z.z-camera.position.z,k=65536.f/6.28318530718f;
            camera.angles[0]=int(-std::atan2(dy,std::sqrt(dx*dx+dz*dz))*k)&65535;camera.angles[1]=(32768-int(std::atan2(dx,dz)*k))&65535;camera.angles[2]=0;camera.perspective=9000;
            auto hit=[&](int damage){const auto at=*w.position(1,index);return w.damage(index,3,damage,0x10,{at.x,at.y+10,at.z},{0,0,-1});};
            for(unsigned t=0;t<420;++t){if(t==10)hit(5);if(t==40||t==48)hit(3);if(t==90)hit(10000);w.tick();
                if(t==9||t==11||t==13||t==42||t==52||t==60||t==89||t==95||t==105||t==115||t==129||t==250||t==419){Scene composed=stage;w.append(composed,{},camera);SoftwareRenderer image(640,480);image.draw(composed,camera);image.save_bmp(output/("side"+std::to_string(side)+"-t"+std::to_string(t)+".bmp"));}}}
    }
    std::cout<<"original en01 damage reaction and death motions passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
