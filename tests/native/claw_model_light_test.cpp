// bhEff30cDrw (effsub3.c:3600, Effekt 312) am Original-Tyrant EN19 in RM_5840: Licht des
// Klauenmodells in beiden bhDrawEffect-Aufrufen (game.c:348 und :427 aus bhEtcMirrorDrawModel).
// Die Klauen entstehen ueber den echten Weg bhEne19 :239-275 (status 0x80000000 des Typ-2-Tyrant,
// bhEne19_SetLeftClaw en19.c:4780). Spiegelebene, pt_flg, abgeschaltete Lichter (Opcode 0x35) und die
// nichtweisse Ambientpalette (Opcode 0x44) sind Diagnosevorgaben, keine natuerlich gespielte Szene.
// Erwartungen stammen aus den Originalformeln: Ninja-Modus 1 (ps2_NinjaCnk.c:590/180) rechnet je
// Eckpunkt fIr = Summe der Multi-Lichter (:1226-1262, ohne Normalisierung) und DifSpe2Amb
// (ps2_Vu1Strip.c:1829) clamp(fIr + AmbientEm, 0, 1) mit AmbientEm = amb * Materialchunk (:946);
// ohne Lichter bleibt also amb[slot] * Material: amb_chr (game.c:344) bzw. amb_rom (game.c:410) im Spiegel.
#include "world.h"
#include "room.h"
#include "effect_rap_claw.h"
#include "binary_view.h"
#include "en19.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<0.0002f;}
static bool point(NJS_POINT3 a,NJS_POINT3 b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static void unit_checks(){
    // prepare_eff30c_model: Modus 1 (EasyMulti) -> Clamp aus Streifenflag 8 allein (Spalte 0 der
    // Sprungtabelle), Kategorie 5, Phase 6/10, Spiegelart 0/3; Farbe/Blendregister unangetastet.
    Scene obj;obj.triangles.resize(3);
    obj.triangles[0].cnk_strip_head=8;obj.triangles[1].cnk_strip_head=8|16;obj.triangles[2].cnk_strip_head=0;
    for(auto& t:obj.triangles){t.blend_register=0x44;t.color=0x4080a0c0;t.light_category=4;}
    obj.ambient_writes.push_back({});
    prepare_eff30c_model(obj,false);
    check(obj.triangles[0].texture_cache_clamp&&obj.triangles[1].texture_cache_clamp&&!obj.triangles[2].texture_cache_clamp,"EasyMulti strip route: flag 8 clamps without flag 16");
    for(const auto& t:obj.triangles)check(t.light_category==5&&t.material_phase==6&&!t.after_modifiers&&t.mirror==0&&!t.camera_mask,"normal call metadata");
    check(obj.ambient_writes[0].phase==6&&!obj.ambient_writes[0].camera_mask,"ambient writes follow phase 6");
    prepare_eff30c_model(obj,true);
    for(const auto& t:obj.triangles)check(t.light_category==5&&t.material_phase==10&&t.after_modifiers&&t.mirror==3,"mirror call metadata");
    check(obj.ambient_writes[0].phase==10,"ambient writes follow phase 10");
    for(const auto& t:obj.triangles)check(t.blend_register==0x44&&t.color==0x4080a0c0,"metadata must not replace material/blend");
}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene stage;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data,const char* room="RM_5840"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),stage(decode_scene(bytes)),actor(stage.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,stage,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
        world.set_presentation_flags(0x10); // nur bhDrawEffect (game.c:339/347/426)
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK& tyrant(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==19)return *t.slot[i];throw std::runtime_error("original RM_5840 tyrant missing");}
    // Lichter ueber den echten Opcode abschalten, Ambientpalette nichtweiss und je Slot verschieden.
    void dark_palette(){
        for(unsigned i=0;world.light_state(i);++i){EventCommand c;c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),0};check(world.bind(c),"disable room light");}
        for(unsigned i=0;world.light_state(i,true);++i){EventCommand c;c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),1};check(world.bind(c),"disable event light");}
        for(unsigned i=0;i<4;++i){EventCommand c;c.opcode=0x44;c.bytes={0x44,std::uint8_t(i+1),std::uint8_t(i+2),std::uint8_t(i+3),std::uint8_t(i)};check(world.bind(c),"set ambient palette");}
    }
    std::vector<SceneTriangle> claws(){Scene out;world.append(out,{},stage.cameras.at(0));std::vector<SceneTriangle> r;
        for(const auto& t:out.triangles)if(t.light_category==4&&!t.camera_mask)r.push_back(t);
        for(const auto& g:world.unbound_enemy_routines())check(g.find("bhEff30cDrw")==std::string::npos,"claw draw still reports a gap");return r;}
    // amb_rom (Slot-Byte 0x3b8, Kategorie 0) im Spiegelaufruf, sonst amb_chr (0x3b9, Kategorie 1).
    void numeric_light(const std::vector<SceneTriangle>& triangles){
        const auto header=inspect_room(bytes).room_offset;BinaryView v(bytes);bool nonwhite=false;
        for(const auto& t:triangles){const unsigned slot=v.u8(header+0x3b8+(t.mirror==3?0u:1u));
            const NJS_POINT3 ambient{float(slot+1)*.1f,float(slot+2)*.1f,float(slot+3)*.1f};
            const auto mat=t.chunk_ambient?t.ambient_material:NJS_POINT3{1,1,1};
            const NJS_POINT3 expected{std::clamp(ambient.x*mat.x,0.f,1.f),std::clamp(ambient.y*mat.y,0.f,1.f),std::clamp(ambient.z*mat.z,0.f,1.f)};
            check(t.world_lit&&t.gouraud,"claw model must enter the late MODE-1 lighting");
            for(const auto& vertex:t.vertices)check(point(vertex.light,expected),"numeric ambient/material light of the claw differs");
            nonwhite=nonwhite||!point(expected,{1,1,1});}
        check(!triangles.empty()&&nonwhite,"numeric light fixture must not degenerate to white");
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();unit_checks();const std::filesystem::path data=argv[1];
    Fixture f(data);f.tick();f.tick(); // bhEne19 :239-275 legt im zweiten Pass die drei 312-Saetze an
    unsigned n312=0;for(unsigned i=0;i<512;++i){const auto* s=f.world.effect_state(i);if(s&&(s->flg&1u)&&s->id==312&&s->custom_queued&&s->claw30c)++n312;}
    check(n312==3,"three queued bhEff30c claws from the original Tyrant route");
    f.dark_palette();
    f.events.flags[5][0]&=~0x100u;const auto normal=f.claws();
    check(!normal.empty(),"claw models drawn in the first bhDrawEffect");
    std::set<unsigned> objects;for(const auto& t:normal)objects.insert(t.object);
    check(objects.size()==3,"objP[10..12] of all three claws");
    for(const auto& t:normal)check(t.mirror==0&&t.material_phase==6&&!t.after_modifiers,"first call phase 6, unmirrored");
    f.numeric_light(normal);
    // Spiegelpass (st_flg 0x100, Ebene mit d != 0 wegen des undefinierten njMirror-Zweigs bei d == +0).
    effect_mirror_plane()={0,2,0,0,1,0};f.events.flags[5][0]|=0x100;
    const auto mirrored=f.claws();
    check(mirrored.size()==2*normal.size(),"mirror pass draws every claw a second time (no mdflg gate in bhEff30cDrw)");
    for(std::size_t i=0;i<normal.size();++i){const auto& a=normal[i];const auto& b=mirrored[i];const auto& c=mirrored[i+normal.size()];
        check(b.mirror==0&&b.material_phase==6&&c.mirror==3&&c.material_phase==10&&c.after_modifiers,"normal then mirrored claw phases");
        check(a.color==b.color&&a.color==c.color&&a.blend_register==c.blend_register&&a.texture_cache_clamp==c.texture_cache_clamp,"material, blend and strip route evaluated in each call");
        for(unsigned k=0;k<3;++k){check(point(a.vertices[k].position,c.vertices[k].position),"vertices must not be pre-reflected");
            check(point(a.vertices[k].normal,c.vertices[k].normal),"normals stay in model/world space; the renderer owns njMirror");}}
    f.numeric_light(mirrored);
    // ef_flg 0x4 (bhSetHalfLight, light.c:992) halbiert nur Lichtfarben, nie die Umgebung: ohne Lichter gleich.
    f.world.set_effect_flags(0x4);const auto half=f.claws();check(half.size()==mirrored.size(),"half light keeps both calls");
    for(std::size_t i=0;i<half.size();++i)for(unsigned k=0;k<3;++k)check(point(half[i].vertices[k].light,mirrored[i].vertices[k].light),"bhSetHalfLight must not halve the ambient term");
    f.world.set_effect_flags(0);
    // Streifenroute: EasyMulti (Spalte 0) klemmt mit Flag 8 auch ohne Flag 16; SimpleMulti (364) wuerde Flag 16 verlangen.
    bool routed=false;for(const auto& t:normal)if(t.cnk_strip_head>=0){check(t.texture_cache_clamp==bool(unsigned(t.cnk_strip_head)&8u),"EasyMulti texture clamp route");routed=true;}
    check(routed,"claw strips must carry their chunk head for routing");
    // Der Zeichenaufruf veraendert den Satz nicht (keine Steuerlogik in bhEff30cDrw).
    for(unsigned i=0;i<512;++i){const auto* s=f.world.effect_state(i);if(s&&(s->flg&1u)&&s->id==312)check(s->claw30c->mode==1&&s->claw30c->erase==-1,"draw must not advance R0_WK state");}
    std::cout<<"claw model light passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"claw model light failed: "<<e.what()<<'\n';return 1;}}
