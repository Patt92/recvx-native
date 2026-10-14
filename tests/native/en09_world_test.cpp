// Original RM_1060 spawns, actual RoomWorld services. State/position writes
// below are diagnostic preconditions, not naturally played progression.
#include "world.h"
#include "room.h"
#include "en09.h"
#include "ninja_math.h"
#include "effect_sprites.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room="RM_1060",unsigned spawn=0):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(spawn)),audio(data),player(PlayerAsset::load(data,10)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(false);
        events.values[15]=spawn; /* runtime.cpp: bhCmpB key 15 = Eintrittsnummer */
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    RECVX_ENEMY_WORK& enemy(unsigned i=0){auto* p=world.enemy_table().slot[i];check(p&&p->id==9,"original RM_1060 Bandersnatcher missing");return *p;}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();}
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable(); /* runtime startup builds the njSin table */const std::filesystem::path data=argv[1];
    {
        Fixture f(data);auto& e=f.enemy();check(e.exp0&&e.mode0==1&&e.mtn_no==1,"EN09 initialization/dispatch absent");
        check(e.hp>=105&&e.hp<=150,"normal HP table selection wrong");
        const auto* exp=static_cast<const RECVX_EN09_EXT*>(e.exp0);float arms[4];
        for(unsigned j=0;j<4;++j){arms[j]=f.world.state(1,0)->enemy_obj.at(7+j).position.x;check(arms[j]==exp->arm_len[j],"Init must capture real arm segment lengths");}
    }
    {
        Fixture f(data);auto& e=f.enemy();const float px=e.px,pz=e.pz;
        for(unsigned i=0;i<400;++i)f.tick();
        check(e.mtn_add!=0&&e.frm_no!=0,"motion clock does not advance");
        check(std::hypot(e.px-px,e.pz-pz)>1,"foot-fix walking does not move actual Work");
        check(f.world.state(1,0)->enemy_calculated,"model tree never calculated");
        // Motion/pose must stay frozen when the original enemy pass is paused.
        const auto frm=e.frm_no;const float x=e.px,z=e.pz;auto pose=f.world.state(1,0)->enemy_obj;
        f.events.flags[6][0]&=~2u;for(int i=0;i<12;++i)f.tick();
        check(e.frm_no==frm&&e.px==x&&e.pz==z,"paused enemy pass advanced");
        const auto& after=f.world.state(1,0)->enemy_obj;check(after.size()==pose.size(),"paused tree changed size");
        for(unsigned j=0;j<pose.size();++j)check(after[j].angles==pose[j].angles&&after[j].position.x==pose[j].position.x,"paused pose was regenerated");
    }
    {
        Fixture f(data);auto& e=f.enemy();auto* exp=static_cast<RECVX_EN09_EXT*>(e.exp0);
        // Type 10 is deliberately idle in the original (MVType10 / MV00).
        e.type=10;e.mode0=1;e.mode2=0;e.mode3=0;e.mtn_no=1;
        for(int i=0;i<500;++i)f.tick();check(e.mode1==0&&e.mode2==0,"type 10 replaced with chase behavior");
        // Source en09.c:6924 DD00 sets death flags and keeps a real clip.
        e.type=0;e.hp=-1;e.mode0=4;e.mode1=e.mode2=e.mode3=0;e.mtn_no=53;
        f.tick();check((e.flg&2)&&!(e.flg&0x28)&&e.mtn_no==56,"DD00 original corpse initialization wrong");
        check(e.ct1>=10&&e.ct1<=14,"DD00 corpse cycle RNG range wrong");
        (void)exp;
    }
    {
        Fixture f(data);auto& e=f.enemy();
        // Source en09.c:2484/2791: dam[] -> total_dam -> actual HP, not host amount.
        e.wpnr_no=5;e.comb_wep=5;e.dam[4]=10;e.flg|=4;const int hp=e.hp;
        f.tick();check(e.hp<hp,"damage accumulation never reaches EN09 HP");
        check(!(e.flg&4)&&e.dam[4]==0,"damage flags not cleared at end of enemy pass");
    }
    {
        // event.c:12053 Common_controll 7 setzt work->flr_no = bhCheckFloorNum(py);
        // der folgende Sub_controll 0x8f (bhLoadWork-Uebernahme) darf die Etage
        // der Raumtabelle (RM_2000 EN09 #1: flr_no -1, py -36) nicht zuruecklegen.
        // Eintritt 0: Skript legt #1 auf (40, 0, 45); Eintritt 1: auf (145, 36, 60).
        for(unsigned spawn=0;spawn<2;++spawn){
            Fixture f(data,"RM_2000",spawn);
            for(unsigned i=0;i<2;++i){auto* w=f.world.enemy_table().slot[i];check(w&&w->id==9,"original RM_2000 Bandersnatcher missing");
                check(w->flr_no==f.world.floor_at({w->px,w->py,w->pz}),"script placement kept the room-table floor instead of bhCheckFloorNum(py)");}
            check(f.world.enemy_table().slot[1]->flr_no==(spawn?f.world.floor_at({145,36,60}):0),"RM_2000 EN09 #1 floor after script placement wrong");
        }
        // event.c:10053 Common_controll 47 = bhCalcModel(work): owP folgt der Skriptlage.
        // Diagnoseaufstellung: #1 wird hier direkt per bhEneSetCk-Befehl 0x24
        // aktiviert (natuerlich: Kameraschnitt-Ereignis). Ohne Neuberechnung zog
        // bhCalcFixOffset den Work auf die Raumtabellenlage (25.03/69.95) zurueck.
        Fixture f(data,"RM_2000",0);auto* w=f.world.enemy_table().slot[1];
        check(w->px==40&&w->pz==45,"RM_2000 EN09 #1 script position (event.c:12034) missing");
        EventCommand on;on.opcode=0x24;on.bytes={0x24,1,1,1};check(f.world.bind(on),"bhEneSetCk activation not bound");
        for(int i=0;i<4;++i)f.tick();
        check(!(w->stflg&0x1000000u)&&std::hypot(w->px-40.0f,w->pz-45.0f)<3.0f,"activated EN09 jumped back to the room-table position (stale owP after Common_controll 47)");
    }
    {
        // effsub4.c:2149/3177: delayed smoke uses model texture, mode1 type,
        // fifteen frames then release; no drawing during the delay.
        EffectSprite s;EffectSpawn spawn;spawn.id=369;spawn.type=1;spawn.scale={2,2,2};initialize_effect_sprite(s,spawn);
        s.xn=1;s.yn=.05f;s.zn=.9f;s.ct3=0;s.tex_id=10;s.stflg=0x20;bool queued=false;
        step_effect_sprite(s,EffectContext{},&queued);check(!queued&&s.mode0==1,"smoke init queued early");
        step_effect_sprite(s,EffectContext{},&queued);check(queued&&s.mode0==2&&s.ct1==1,"smoke start/animation clock wrong");
        check(std::abs(s.sx-.32f)<1e-6f&&std::abs(s.tv[1].u-15.f/255)<1e-6f,"smoke UV/scale uses wrong domain");
        for(int i=1;i<15;++i)step_effect_sprite(s,EffectContext{},&queued);
        step_effect_sprite(s,EffectContext{},&queued);check(!queued&&!s.flg,"smoke fails to release");
    }
    std::cout<<"EN09 real-world dispatch, arm initialization, walking, pause, type 10, corpse, damage, RM_2000 script floor and smoke PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
