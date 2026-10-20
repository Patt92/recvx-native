/* bhControlLight light.c:737-765 auf dem vorhandenen eff[512]-Besitzer.
   Die Fixture injiziert Work-/Tabellenzustaende; kein natuerlicher Raumablauf.
   Alle Erwartungen sind Originalformeln/-reihenfolgen, keine neu gesampelte Pose.
   Einzeilen-Gegenproben (je einzeln, Hauptbuild, Test ROT, zurueck + touch):
   1. light_effect_slot: return index; -> "efid mapping"
   2. light_effect_matrix node 0: njUnitMatrix statt behaltenem effect_root -> "allocation zero"
   3. control_lights: Todpruefung vor animate_light_type -> "dead target RNG"
   4. Sprite-root: njScale vor copy_n -> "unscaled root"
   5. light_effect_matrix node > 0: return work.effect_root -> "own owP"
   6. light_state: link_point(kind 4) statt l.position fuer lkflg 5 -> "allocation zero"
   7. world.cpp trs-Schleife: in parent-Berechnung UND Root-Bedingung !(flg&0x200000) entfernen -> "flg 0x200000 precedence"
*/
#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return std::fabs(a.x-b.x)<1e-3f&&std::fabs(a.y-b.y)<1e-3f&&std::fabs(a.z-b.z)<1e-3f;}
namespace recvx {
// Ein Freund nur dieses Tests; kein oeffentlicher Host-Override fuer neue Matrizen.
struct LightEffectLinkTestAccess {
    static void clean(RoomWorld& w){
        w.sprites_.clear();w.room_effect_slots_.clear();
        for(auto& l:w.lights_)l.flags=0;for(auto& l:w.event_lights_)l.flags=0;
        w.unbound_enemy_.clear();w.enemy_ai_=false;
    }
    static EffectSprite& sprite(RoomWorld& w,unsigned slot){
        if(w.sprites_.size()<=slot)w.sprites_.resize(slot+1);
        w.sprites_[slot]=RoomWorld::SpriteWork{};w.sprites_[slot].scale={1,1,1};
        auto& s=w.sprites_[slot].sprite;s.flg=1;s.id=5;s.mode0=1;s.slot=int(slot);
        w.entity(4,slot)=WorldEntity{};return s;
    }
    static void map(RoomWorld& w,std::initializer_list<unsigned> slots){w.room_effect_slots_=slots;}
    static void root(RoomWorld& w,unsigned slot,NJS_POINT3 p,int ay=0){auto& m=w.sprites_.at(slot).effect_root.value;njUnitMatrix(&m);njTranslate(&m,p.x,p.y,p.z);njRotateY(&m,ay);}
    static void light(RoomWorld& w,unsigned index,unsigned node,NJS_POINT3 offset,bool event=false,unsigned type=0){
        auto& table=event?w.event_lights_:w.lights_;if(table.size()<5)table.resize(5);
        auto& l=table[4];l=RoomWorld::Light{};l.flags=3;l.type=type;l.speed=0;l.link=5;l.index=index;l.node=node;l.source=4;
        l.offset=offset;l.position={91,92,93};l.color={1,1,1};l.direction={7,8,9};l.near_range=100;l.far_range=200;
        l.working_position={81,82,83};l.working_direction=l.direction;l.working_color=l.color;l.color_sampled=true;
        l.working_near_range=100;l.working_far_range=200;l.mode=type==10?1u:0u;
        w.event_camera=event;
    }
    static void ready(RoomWorld& w,unsigned slot){auto& work=w.sprites_.at(slot);auto& s=work.sprite;
        s.tex_id=5;s.ani_ct=0;s.flg=1;s.pn=4;s.bl_src=8;s.bl_dst=6;work.queued=true;work.hidden=false;
        for(auto& v:s.tv)v.col=0xffffffffu;
        SceneTexture t;t.width=1;t.height=1;t.pixels={0xffffffffu};w.effect_textures_[5]={t};
        auto& e=w.entity(4,slot);e.transform.position={s.px,s.py,s.pz};e.angles={s.ax,s.ay,s.az};
    }
    static bool gap(const RoomWorld& w,const char* text){for(const auto& g:w.unbound_enemy_)if(g.find(text)!=std::string::npos)return true;return false;}
    static void hide(RoomWorld& w,unsigned slot,bool hidden){w.sprites_.at(slot).hidden=hidden;w.sprites_[slot].sprite.stflg=hidden?0x1000000u:0u;}
    static std::optional<BoneMatrix> matrix(const RoomWorld& w,unsigned slot,unsigned node){return w.light_effect_matrix(slot,node);}
    static NJS_POINT3 saved(const RoomWorld& w){return w.lights_[4].working_position;}
    static void position(RoomWorld& w,unsigned slot,NJS_POINT3 p){w.entity(4,slot).transform.position=p;}
    static void end_type(RoomWorld& w){w.lights_[4].type=13;w.lights_[4].speed=1;w.lights_[4].clock=16384;}
    // Raumentity-Link kind 2 (obwp, link 3) als vorhandener Elternbesitzer; nur fuer den Vorrangfall.
    static void parent(RoomWorld& w,unsigned slot,unsigned link,unsigned index,unsigned node){auto& work=w.sprites_.at(slot);work.link=link;work.index=index;work.node=node;work.sprite.link_kind=int(link)-1;work.sprite.link_index=index;}
    static void own_tree(RoomWorld& w,unsigned slot,const PlayerAsset& player,unsigned node){
        const auto* asset=player.model(4);check(asset&&asset->bones()>node,"fixture own model unavailable");
        auto& e=w.entity(1,0);e.enemy_asset=&player;e.enemy_calculated=false;
        auto& s=w.sprites_[slot].sprite;s.model_enemy=0;s.model_mdl=4;
        auto pose=asset->bind_pose();std::vector<BoneMatrix> nodes(asset->bones());std::vector<std::uint32_t> flags(asset->bones());
        for(auto& m:nodes){njUnitMatrix(&m.value);njTranslate(&m.value,20,30,40);}
        njRotateY(&nodes[asset->model_index(node)].value,16384);
        w.commit_afterimage_model(1,0,4,*asset,pose,nodes,flags);
    }
};
}
using Access=LightEffectLinkTestAccess;
// Nur Daten: alle 205 Dateien, keine RoomWorld-Konstruktion und keine Raum-Ticks.
static void room_tables(const std::filesystem::path& data){
    unsigned rooms=0,lgt=0,evl=0,effects=0,link5=0;
    for(const auto& entry:std::filesystem::directory_iterator(data/"rooms")){
        if(entry.path().extension()!=".RDT")continue;++rooms;
        const auto bytes=read_asset_file(entry.path(),64*1024*1024);const BinaryView v(bytes);const auto h=inspect_room(bytes).room_offset;
        const auto ep=v.u32(h+0x14),en=v.u32(h+0x94);v.require(ep,std::size_t(en)*68);
        for(unsigned i=0;i<en;++i){check((v.u32(ep+i*68)&3u)==1u,"room effect slot identity precondition");++effects;}
        for(const bool event:{false,true}){const auto p=v.u32(h+(event?0x3c:4)),n=v.u32(h+(event?0xb4:0x84));v.require(p,std::size_t(n)*224);
            (event?evl:lgt)+=n;for(unsigned i=0;i<n;++i)if(v.u32(p+i*224+12)==5)++link5;}
    }
    check(rooms==205&&lgt==3122&&evl==2017&&effects==3171&&link5==0,"205 original room table census changed");
}
static void with_world(const std::filesystem::path& data,const std::function<void(RoomWorld&,const Scene&)>& body){
    auto bytes=read_asset_file(data/"rooms/RM_0000.RDT",64*1024*1024);auto stage=decode_scene(bytes);auto actor=stage.spawns.at(0);
    std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0000");
    Access::clean(world);body(world,stage);
}
static void roots(const std::filesystem::path& data){with_world(data,[&](RoomWorld& w,const Scene& stage){
    auto& s=Access::sprite(w,7);Access::map(w,{7});Access::light(w,0,0,{1,2,3});
    s.px=50;s.py=60;s.pz=70;Access::position(w,7,{500,600,700});
    w.control_lights();check(same(w.light_state(4)->position,{0,0,0}),"allocation zero: op->mtxbuf must not be identity/T(px)");
    Access::root(w,7,{10,20,30},16384);w.control_lights();
    check(same(w.light_state(4)->position,{13,22,29}),"efid mapping and rotated local light offset");
    check(same(w.light_state(4)->direction,{7,8,9}),"link transforms position, never light direction");
    Access::hide(w,7,true);w.control_lights();check((w.light_state(4)->flags&3u)==3u,"hidden active effect is not a dead target");
    Access::hide(w,7,false);
    w.set_simulation_flags(0);const auto before=Access::saved(w);Access::root(w,7,{20,30,40},16384);w.control_lights();
    check(same(w.light_state(4)->position,{23,32,39})&&same(Access::saved(w),before),"paused link moves lp->px, not wpx");
    Access::light(w,0,0,{1,2,3},true);w.control_lights();check(same(w.light_state(4,true)->position,{23,32,39}),"event light shares effect link mechanism");
    w.event_camera=false;Access::light(w,9,0,{1,2,3});w.control_lights();check(Access::gap(w,"sys->efid[lkno]"),"invalid efid must report, not clamp");
    Access::light(w,0,0,{1,2,3});s.mode0=1;s.id=148;w.control_lights();
    check(Access::gap(w,"eigener op->mtx-Schreiber"),"unpublished procedure root must remain named gap");
    (void)stage;
});}
static void death_reuse(const std::filesystem::path& data){with_world(data,[&](RoomWorld& w,const Scene&){
    auto& s=Access::sprite(w,7);Access::map(w,{7});Access::root(w,7,{10,20,30});Access::light(w,0,0,{1,2,3},false,10);
    // Typ 10, mode 1, aspd 0: genau ein rand() je Pass (light.c:494-505), auch bei totem Ziel.
    s.flg=2;w.set_simulation_flags(0x80u);recvx_core_random_seed(77);(void)recvx_core_random_next();const int expected=recvx_core_random_next();recvx_core_random_seed(77);
    w.control_lights();check(recvx_core_random_next()==expected,"dead target RNG: light type executes BEFORE target death");
    check((w.light_state(4)->flags&3u)==0,"flg 2 reservation is inactive, must permanently clear light bits 0/1");
    s.flg=1;w.control_lights();check((w.light_state(4)->flags&3u)==0,"target revival must not relight dead light");
    s.flg=0;const int reused=w.reserve_effect_slot(7);check(reused==7,"fixture recycle wrong slot");
    Access::position(w,7,{100,200,300});auto* fresh=const_cast<EffectSprite*>(w.effect_state(7));fresh->flg=1;
    Access::light(w,0,0,{3,2,1});w.control_lights();check(same(w.light_state(4)->position,{0,0,0}),"slot reuse keeps efid, clears previous root");
    Access::root(w,7,{5,6,7});Access::end_type(w);w.control_lights();check(!(w.light_state(4)->flags&1u)&&same(w.light_state(4)->position,{8,8,8}),"type expiration still resolves active link in same pass");
});}
static void draw_publication(const std::filesystem::path& data){with_world(data,[&](RoomWorld& w,const Scene& stage){
    auto& s=Access::sprite(w,7);s.px=10;s.py=20;s.pz=30;s.ay=16384;s.sx=7;s.sy=8;s.sz=9;Access::ready(w,7);Access::map(w,{7});Access::light(w,0,0,{1,2,3});
    w.set_presentation_flags(0x10u);w.control_lights();check(same(w.light_state(4)->position,{0,0,0}),"no draw must keep initial root zero");
    Scene draw;w.append(draw,{},stage.cameras.at(0));w.control_lights();
    check(same(w.light_state(4)->position,{13,22,29}),"unscaled root: actual sprite draw must publish matrix before scale");
    s.px=100;s.py=200;s.pz=300;Access::position(w,7,{100,200,300});
    w.control_lights();check(same(w.light_state(4)->position,{13,22,29}),"control must not invent next effect pose");
    w.set_presentation_flags(0);Scene stopped;w.append(stopped,{},stage.cameras.at(0));w.control_lights();
    check(same(w.light_state(4)->position,{13,22,29}),"disabled draw pass must retain last matrix");
    w.set_presentation_flags(0x10u);Access::hide(w,7,true);Scene hidden;w.append(hidden,{},stage.cameras.at(0));w.control_lights();
    check(same(w.light_state(4)->position,{13,22,29}),"hidden draw must retain last matrix");
    Access::hide(w,7,false);s.flg|=0x100000u; // Billboard affects draw basis, not op->mtx
    Scene next;w.append(next,{},stage.cameras.at(0));
    check(same(w.light_state(4)->position,{13,22,29}),"light state is the latched bhControlLight result, not a draw-time resolve");
    w.control_lights();check(same(w.light_state(4)->position,{103,202,299}),"next light pass sees last real draw root, not billboard");
    // Snapshot save/restore includes SpriteWork.mtx, not a side registry.
    EventCommand push;push.opcode=0x70;check(w.bind(push),"push effect work binding");Access::root(w,7,{1,1,1});
    EventCommand pop;pop.opcode=0x71;check(w.bind(pop),"pop effect work binding");w.control_lights();
    check(same(w.light_state(4)->position,{103,202,299}),"effect stack must restore retained root");
    // effect.c:1335: flg 0x200000 waehlt T(px) auch bei flg 0x80 mit aufloesbarem Link.
    s.flg=1|0x80u|0x200000u;s.lox=500;s.loy=500;s.loz=500;Access::parent(w,7,1,0,0);Access::position(w,7,{40,50,60});
    Scene precedence;w.append(precedence,{},stage.cameras.at(0));w.control_lights();
    check(same(w.light_state(4)->position,{43,52,59}),"flg 0x200000 precedence: root must be T(px) RotXYZ, not the parent node");
});}
static void own_nodes(const std::filesystem::path& data){const auto player=PlayerAsset::load(data,10);with_world(data,[&](RoomWorld& w,const Scene&){
    auto& s=Access::sprite(w,7);Access::map(w,{7});Access::root(w,7,{500,600,700});
    NJS_MATRIX parent;njUnitMatrix(&parent);njTranslate(&parent,1000,2000,3000);s.link_matrix=parent;
    Access::light(w,0,1,{1,2,3});w.control_lights();
    check(Access::gap(w,"ohne veroeffentlichten nativen Baumbesitzer")&&!Access::matrix(w,7,1),"own owP: never substitute parent or root");
    Access::own_tree(w,7,player,1);w.control_lights();
    check(same(w.light_state(4)->position,{23,32,39}),"own owP: read real published selected model tree");
    const auto root=Access::matrix(w,7,0);check(root&&root->value[12]==500&&root->value[13]==600&&root->value[14]==700,"root and own node must remain distinct");
    s.link_matrix=nullptr;
});}
int main(int argc,char** argv){try{
    check(argc==2,"usage: light_effect_link_test <data>");_Make_SinTable();const std::filesystem::path data=argv[1];
    room_tables(data);roots(data);death_reuse(data);draw_publication(data);own_nodes(data);
    std::cout<<"PASS: effect light link (synthetic work fixture + 205 room tables)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
