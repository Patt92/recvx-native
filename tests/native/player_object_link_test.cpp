// event.c:5263 bhObjLinkSetObjPly (Skriptbefehl 0x6F) und MdlPut.c:242 bhCalcModel(plp) mit
// flg 0x80: die Spielerwurzel ist obwp[lkwkp].owP[lkono].mtx * T(lox,loy,loz) * RXYZ(ax,ay,az).
// Echte Bytes aus RM_4011: Ereignis 35 (Offset 1046) haengt die Spielerin an Objekt 6 Knoten 3 mit
// (-6.50, -9.20, -1.70), Ereignis 11 (Offset 5366) loest sie. Vorher uebernimmt das Skript die
// Spielerin mit bhLoadWork 0 (0x65 00 00 00), dieselben Bytes stehen hier davor.
#include "world.h"
#include "room.h"
#include "binary_view.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
static bool same(const NJS_MATRIX& a,const NJS_MATRIX& b){for(unsigned i=0;i<16;++i)if(std::fabs(a[i]-b[i])>1e-3f)return false;return true;}
static bool gap(const RoomWorld& w,const char* t){for(const auto& g:w.unbound_enemy_routines())if(g.find(t)!=std::string::npos)return true;return false;}
int main(int argc,char** argv){try{
    check(argc==2,"data directory required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_4011.RDX",64*1024*1024);auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);auto player=PlayerAsset::load(data);
    Inventory inv;MessageDialog dialog;EventVm events(bytes,inv,dialog);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_4011");world.set_player_asset(&player);world.bind_player_collision(events);
    world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
    const auto base=inspect_room(bytes).event_offset;const BinaryView v(bytes);
    const auto command=[&](std::size_t at,std::uint8_t rel){
        const std::uint8_t expect[]={0x6F,6,0,3,rel,7};for(unsigned i=0;i<6;++i)check(v.u8(base+at+i)==expect[i],"RM_4011: 0x6F nicht an der erwarteten Stelle");
        return EventCommand{0x6F,0,0,base+at,{bytes.begin()+long(base+at),bytes.begin()+long(base+at+12)}};};
    check(world.state(2,6),"RM_4011 Objekt 6 fehlt");
    // Ohne Bindung: Wurzel aus plp->px/ay.
    std::vector<BoneMatrix> free_nodes;(void)world.sample_player(player,&free_nodes);
    check(world.bind({0x65,0,0,0,{0x65,0,0,0}})&&world.player_scripted,"bhLoadWork 0 nicht gebunden");
    check(world.bind(command(1046,0)),"0x6F (anhaengen) nicht gebunden");
    const auto& l=world.player_object_link();
    check(l.linked&&l.object==6&&l.node==3,"lkwkp/lkono/flg 0x80 falsch");
    check(std::fabs(l.offset.x+6.50f)<1e-5f&&std::fabs(l.offset.y+9.20f)<1e-5f&&std::fabs(l.offset.z+1.70f)<1e-5f,"lox/loy/loz oder Vorzeichen falsch");
    check(events.flags[13][0]&0x80u,"plp->flg 0x80 nicht gesetzt");
    // Erwartete Wurzel unabhaengig zusammengesetzt; Knoten 0 = Wurzel * T(objP[0].pos) * RXYZ(objP[0].ang).
    const auto parent=world.object_node_matrix(6,3);check(parent.has_value(),"Objekt 6 Knoten 3 ohne Matrix");
    NJS_MATRIX root;std::copy_n(parent->value,16,root);njTranslate(&root,l.offset.x,l.offset.y,l.offset.z);njRotateXYZ(&root,actor.pitch,actor.yaw,actor.roll);
    const auto local=player.local_pose(motion,frame);NJS_MATRIX expect;std::copy_n(root,16,expect);
    njTranslate(&expect,local[0].position.x,local[0].position.y,local[0].position.z);njRotateXYZ(&expect,local[0].angles[0],local[0].angles[1],local[0].angles[2]);
    std::vector<BoneMatrix> linked_nodes;(void)world.sample_player(player,&linked_nodes);
    check(!linked_nodes.empty()&&same(linked_nodes[0].value,expect),"Wurzelknoten folgt nicht obwp[6].owP[3] * T(lo) * R");
    check(!same(linked_nodes[0].value,free_nodes[0].value),"Vorbedingung: verknuepfte und freie Wurzel muessen sich unterscheiden");
    world.tick_after_player();check(!gap(world,"freier Spielerpass mit flg 0x80"),"skriptgesteuerte Bindung darf keine Luecke melden");
    // Loesen mit den echten Bytes: flg 0x80 aus, Wurzel wieder aus plp->px/ay.
    check(world.bind(command(5366,1))&&!world.player_object_link().linked&&!(events.flags[13][0]&0x80u),"0x6F (loesen) loescht flg 0x80 nicht");
    std::vector<BoneMatrix> again;(void)world.sample_player(player,&again);check(same(again[0].value,free_nodes[0].value),"nach dem Loesen nicht wieder die freie Wurzel");
    // Diagnose: Bindung bei freiem Spielerpass wird benannt; obwp 0..3 und fehlende Objekte bleiben ungebunden.
    check(world.bind(command(1046,0)),"erneutes Anhaengen");world.player_scripted=false;world.tick_after_player();
    check(gap(world,"freier Spielerpass mit flg 0x80"),"freier Spielerpass mit Bindung nicht gemeldet");
    check(!world.bind({0x6F,0,0,0,{0x6F,2,0,3,0,0,0,0,0,0,0,0}})&&!world.bind({0x6F,0,0,0,{0x6F,200,0,3,0,0,0,0,0,0,0,0}}),"obwp 0..3 bzw. fehlendes Objekt gebunden");
    std::cout<<"bhObjLinkSetObjPly PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
