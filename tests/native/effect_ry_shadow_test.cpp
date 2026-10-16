// effsub3.c:1640 rySetShadow / :1683 bhEff304 am Original-Tyrant (EN19, RM_5840,
// bhEne19_Init en19.c:374: Gelenke 24/20/2, Zuschlag 2.0/3.0). Der Erwartungswert
// des ersten Passes wird aus denselben owP-Matrizen gerechnet, die der Effektpass
// liest; die Abbildung auf den Modifier-Zeichenweg (sys->efm[1].objP[mdlver]) wird
// ueber scene.shadows geprueft.
#include "world.h"
#include "room.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    auto bytes=RoomArchive(data).load("RM_5840.RDX",64*1024*1024);auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);PlayerAsset player=PlayerAsset::load(data);Inventory inv;MessageDialog dialog;std::uint32_t head=0;
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_5840");EventVm events(bytes,inv,dialog);
    world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
    events.binding=[&](const EventCommand& c){return world.bind(c);};
    world.commit_player_joints(player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw)));
    events.initialize();
    // rySetShadow: AllocOworkOne, flg 0x8240001, id 304, ct0..2 = Gelenke 24/20/2, xn/zn = 2/3.
    int slot=-1;for(unsigned i=0;const auto* s=world.effect_state(i);++i)if(s->id==304&&(s->flg&1u)){slot=int(i);break;}
    check(slot>=0,"rySetShadow legte keinen eff[]-Platz 304 an");
    const auto* s=world.effect_state(unsigned(slot));
    check(s->type==0&&s->tex_id==-1&&s->mdlver==0&&s->ct0==24&&s->ct1==20&&s->ct2==2&&s->xn==2.0f&&s->zn==3.0f,"rySetShadow: Feldbelegung");
    check((s->flg&0x8240000u)==0x8240000u,"rySetShadow: flg 0x8240001");
    // Erster Effektpass: Tyrant-Matrizen sind gerechnet (bhControlEnemy vor bhControlEffect).
    const WorldEntity* tyrant=nullptr;for(unsigned i=0;i<64;++i)if(const auto* e=world.state(1,i);e&&e->enemy_bound&&e->enemy.id==19)tyrant=e;
    check(tyrant,"Original-Tyrant fehlt");
    world.set_simulation_flags(events.flags[6][0]);world.tick();
    const auto& tree=tyrant->model_owner?*tyrant->model_owner:*tyrant;check(tree.enemy_calculated,"Tyrant ohne owP-Matrizen");
    auto joint=[&](unsigned j){const auto* m=tree.enemy_owp_mtx.at(tree.model_window+j).value;return NJS_POINT3{m[12],m[13],m[14]};};
    NJS_POINT3 a=joint(24),b=joint(20),c=joint(2);NJS_POINT3 pp{(a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f};
    float ra=(b.x-a.x)*(b.x-a.x)+(b.z-a.z)*(b.z-a.z),rb=(c.x-pp.x)*(c.x-pp.x)+(c.z-pp.z)*(c.z-pp.z);
    NJS_POINT3 p0=a,p1=b;if(rb>ra){std::swap(ra,rb);p1=pp;p0=c;}
    ra=njSqrt(ra);rb=njSqrt(rb);
    const float sx=(1.0f+(ra+2.0f))*.5f,sz=(1.0f+(rb+3.0f))*.5f;const int ay=-int(10430.381f*std::atan2(p0.z-p1.z,p0.x-p1.x));
    check(std::fabs(s->sx-sx)<1e-4f&&std::fabs(s->sz-sz)<1e-4f,"bhEff304: sx/sz = (alt + Abstand + Zuschlag) / 2");
    check(s->ay==ay,"bhEff304: ay aus atan2(vct.z, vct.x)");
    // Abstand zur alten Lage (0,0,0) groesser als rb -> Lage springt auf die Mitte; type 0 setzt py auf den Boden.
    check(std::fabs(s->px-(p0.x+p1.x)*.5f)<1e-4f&&std::fabs(s->pz-(p0.z+p1.z)*.5f)<1e-4f,"bhEff304: Lage = Mitte der beiden Punkte");
    const bool hidden=(tyrant->enemy.stflg&0x8u)||(tyrant->enemy.stflg&0x1000000u)||(tyrant->enemy.mdflg&0x1u);
    check(bool(s->flg&0x1000000u)==hidden,"bhEff304: Sichtgate aus stflg 0x8/0x1000000, mdflg 0x1");
    world.set_presentation_flags(0x1fu);Scene out;world.append(out,{},scene.cameras.at(0));
    check(hidden||!out.shadows.empty(),"bhDrawMdfEffect: 304 nicht im Modifier-Zeichenweg");
    for(const auto& g:world.unbound_enemy_routines())check(g.find("bhEff304")==std::string::npos&&g.find("rySetShadow")==std::string::npos,"304-Luecke gemeldet");
    std::cout<<"rySetShadow/bhEff304 passed (slot "<<slot<<", sx "<<s->sx<<", sz "<<s->sz<<", hidden "<<hidden<<", shadows "<<out.shadows.size()<<").\n";return 0;
}catch(const std::exception& e){std::cerr<<"effect ry shadow failed: "<<e.what()<<'\n';return 1;}}
