// EN27 Mottenei auf dem Spielerasset (plp->mdl[6]/[7], sys->plmthp, lkwkp = plp)
// im Originalraum RM_6030. Die stflg-Marke vor dem Raumstart (wie nach einem
// Mottengriff, en06.c:1170) und das Verkuerzen von ct2 sind Diagnose-
// vorbedingungen, kein natuerlich gespielter Fortschritt.
// Semantikreferenz en27.c, player.c:1042 bhCheckMothEgg.
#include "world.h"
#include "room.h"
#include "en27.h"
#include "ninja_math.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,std::uint32_t marks):bytes(RoomArchive(data).load("RM_6030.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_6030"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        events.flags[14][0]|=marks; // plp->stflg vor bhCheckMothEgg (player.c:934)
        commit();events.initialize();
        events.flags[6][0]=0xffffffffu; // room.c:997-1009: sp_flg = -1 im Bild nach dem Laden
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){events.tick();world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    std::vector<RECVX_ENEMY_WORK*> eggs(){std::vector<RECVX_ENEMY_WORK*> out;auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n;++i)if(t.slot[i]&&(t.slot[i]->flg&1u)&&t.slot[i]->id==27)out.push_back(t.slot[i]);return out;}
    void no_gaps(const char* phase){
        static const char* allowed[]={"bhEff_E06_Rinpun: op->cspr.c"};
        for(const auto& g:world.unbound_enemy_routines()){bool ok=false;for(const char* a:allowed)if(g.find(a)!=std::string::npos)ok=true;
            if(!ok)throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
};
static const WorldEntity& ent(const RECVX_ENEMY_WORK& w){return *static_cast<const WorldEntity*>(w.owner);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // player.c:862 bhInitRoomChangePlayer: plp->stflg &= 0x78280000 ueber den Raumwechsel.
        const auto bytes=RoomArchive(data).load("RM_6030.RDX",64*1024*1024);Inventory inv;MessageDialog dialog;
        EventVm before(bytes,inv,dialog),after(bytes,inv,dialog);
        before.flags[14][0]=0x48081003u|0x01000000u;after.inherit_progress(before);
        check(after.flags[14][0]==0x48080000u,"Raumwechsel: stflg & 0x78280000");
    }
    {
        // bhCheckMothEgg: je Marke ein Ei mit type 0/1/2, Reihenfolge der Marken.
        Fixture f(data,0x28000000u);auto eggs=f.eggs();
        check(eggs.size()==2&&eggs[0]->type==0&&eggs[1]->type==2,"bhCheckMothEgg: zwei Eier, type 0 und 2");
    }
    Fixture f(data,0x8000000u);
    auto eggs=f.eggs();check(eggs.size()==1,"bhCheckMothEgg: ein Ei fuer 0x8000000");
    auto& e=*eggs[0];const auto& en=ent(e);
    // bhEne27_Init (en27.c:54) im ersten Gegnerpass.
    check(e.mode0==1&&e.mode1==1&&e.mode2==0&&e.mtn_no==93u&&e.ax==5461&&(e.flg&0x80u)&&e.lk_player&&!e.lkwkp&&e.lkono==3,"Init: mode 1/1/0, mtn 93, ax 5461, lkwkp = plp, lkono 3");
    check((e.mdflg&0x4u)&&!(e.mdflg&0x21u)&&e.clp_jno[0]==0&&e.clp_jno[1]==-1,"Init: mdflg 0x4, ohne 0x1/0x20, clp_jno");
    check(en.enemy_asset==f.player.model(6)&&en.model_bank==&f.player&&en.model_base==6u&&en.player_motion_library,"Init: mlwP = &plp->mdl[6], mnwP = sys->plmthp");
    {const auto* clip=f.player.library_motion(93);check(clip&&!clip->positions.empty(),"plmthp[93]");
     const auto& k=clip->positions[0][0];check(e.lox==k.x&&e.loy==k.y&&e.loz==k.z-0.3f,"Init: lo = md2P->p[0] Bild 0, z - 0.3");}
    f.tick();f.tick();
    // bhDrawEnemy (mdflg 0x4): Wurzel = plp->owP[3] * T(lo) * Rot(ax).
    {check(en.enemy_calculated,"bhCalcModel: Ei berechnet");
     const auto joint=f.world.player_committed_joints().at(3);NJS_MATRIX m;std::memcpy(m,joint.value,sizeof m);
     njTranslate(&m,e.lox,e.loy,e.loz);njRotateXYZ(&m,e.ax,e.ay,e.az);
     check(std::fabs(e.root_mtx[12]-m[12])<1e-4f&&std::fabs(e.root_mtx[13]-m[13])<1e-4f&&std::fabs(e.root_mtx[14]-m[14])<1e-4f,"bhCalcModel: Wurzel am Spielergelenk 3");}
    // eneset.c:797 bhDrawEnemy: das Ei zeichnet plp->mdl[6] mit dessen Textur (plp->txp[6]).
    {
     auto draw=[&]{Scene out;f.world.append(out,{},SceneCamera{});return out;};
     e.mdflg|=1u;const Scene without=draw();e.mdflg&=~1u;const Scene with=draw();
     check(with.triangles.size()>without.triangles.size(),"bhDrawEnemy: Ei-Dreiecke fehlen");
     check(with.textures.size()>=without.textures.size()+f.player.model(6)->textures().size(),"bhDrawEnemy: mdl[6].texP fehlt");}
    f.no_gaps("Init");
    // Schluepfen: ct2 laeuft ab (BR00 :170), MV01 haengt mdl[7] ein und greift den Spieler (Motion 90).
    e.ct2=1;f.tick();
    RECVX_ENEMY_PLAYER pl{};check(f.world.enemy_player_get(pl),"Spielerbesitzer fehlt");
    check(e.mode2==2&&en.enemy_asset==f.player.model(7)&&e.mdl_no==8&&e.lox==0&&e.ax==0,"MV01: mlwP = &plp->mdl[7], mdl_no 8, lo/ax 0");
    check(pl.mode0==4&&pl.mtn_no==90u&&(pl.flg&0x10004u)==0x10004u&&(pl.stflg&0x10000u)&&!pl.mtn_enemy,"MV01: Spieler mode0 4, Motion 90 aus mnwPb");
    bool incubate=false;for(unsigned i=0;const auto* s=f.world.effect_state(i);++i)if((s->flg&1u)&&s->id==5&&s->link_kind==0)incubate=true;
    check(incubate,"MV01: IncubateEffect (id 5) am Spieler");
    // MV02: Ei-Motion 93, dann Schlupf 96 ohne Hebel (ct1 <= 30), Schaden 5 + Gift, Freigabe am Bildende 0.
    const int hp0=pl.hp;bool hatched=false,hurt=false,released=false;
    for(int i=0;i<400&&!released;++i){f.tick();f.world.enemy_player_get(pl);
        if(!hatched&&e.mtn_no==96u){hatched=true;check(pl.mtn_no==91u,"MV02: Spielermotion 91 beim Schlupf");}
        if(!hurt&&pl.hp<hp0){hurt=true;check(pl.hp==hp0-5&&(pl.stflg&0x80000u)&&pl.mtn_no==92u,"MV02: hp - 5, Gift, Motion 92");}
        if(hurt&&pl.mode0==1){released=true;check(!(pl.stflg&0x8000000u)&&!(pl.stflg&0x10000u)&&(pl.flg&8u)&&!(pl.flg&0x10004u),"MV02: Freigabe, Marke 0x8000000 geloescht");}}
    check(hatched&&hurt&&released,"Schlupfkette MV02 nicht vollstaendig");
    // Die Larve faellt (flg 0x80 aus) und verschwindet (flg 0).
    bool gone=false;for(int i=0;i<120&&!gone;++i){f.tick();if(!(e.flg&1u))gone=true;}
    check(gone,"MV02 Fall 3: Larve faellt und endet");
    f.no_gaps("Schlupf");
    std::cout<<"EN27 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
