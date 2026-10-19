// Zielaufschaltung und Schusstreffer gegen die Originalregeln.
//
// Teil 1 (ohne Daten): der native Kampfkern (native/player_combat_core.c) auf
// einer Diagnose-Gegnertabelle. Die Flagstaende sind die, die der en01-Port in
// der Runtime schreibt: lebend 0x979 (bhEne01_Init flg |= 0x178, en01.c:2721),
// Sterbefall mode0 3 vor DD (0x40979, hp < 0), tot nach bhEne01_DD00..DD03
// (flg |= 2, flg &= ~0x128 bzw. ~0x28, en01.c:9961/10113/10141/10225).
//   pwksub.c:244 bhSearchNearEnemy: flg 1, 0x20, !2, !flg2 0x10, !stflg 0x41000000
//   weapon.c:614 bhCheckGunAtari:   flg 1, 0x20, !2, !stflg 0x1000000, !flg 4
// Eine tote Arbeit wird weder aufgeschaltet noch getroffen; eine Arbeit im
// Sterbefall (DD noch nicht erreicht) ist im Original noch Ziel. Ein liegender
// Zombie (Kapsel am Boden) wird vom waagerechten Schuss (gap.py = py + 11,
// player.c:5366) nicht beruehrt und verdeckt den stehenden dahinter nicht.
//
// Teil 2 (RM_0080): bhCheckC2WallN (pwksub.c:2172) prueft die Kapsel mit ihrem
// Radius (0.1 an allen Ziel-/Treffstellen: pwksub.c:259/359/544, playpch.c:369,
// weapon.c:688) gegen njCollisionCheckBC. Der fruehere Strahlersatz
// (combat_wall_distance) liess eine Sichtlinie 0.05 neben einer echten Raumwand
// frei; die Bindung des Kampfkerns muss sie wie das Original sperren.
#include "world.h"
#include "asset_io.h"
#include "binary_view.h"
#include "room.h"
#include "player_combat_core.h"
#include "weapon_tables.h"
#include "wall_capsule.h"
#include "ninja_math.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}

namespace {
struct Rules {
    RECVX_ENEMY_TABLE table{};
    std::vector<RECVX_ENEMY_WORK> works;
    std::set<std::string> gaps;
    RECVX_COMBAT_SERVICES services()const{
        RECVX_COMBAT_SERVICES s{};s.user=const_cast<Rules*>(this);
        s.gap=[](void* u,const char* name){static_cast<Rules*>(u)->gaps.insert(name);};
        s.enemies=[](void* u)->RECVX_ENEMY_TABLE*{return &static_cast<Rules*>(u)->table;};
        s.c2_wall_n=[](void*,const RECVX_CB_CAPSULE*,uint32_t)->int32_t{return 0;};   // leerer Raum
        s.l2_wall=[](void*,const RECVX_CB_LINE*,uint32_t,float*,RECVX_CB_WALL_HIT*)->int32_t{return 0;};
        return s;}
    static RECVX_CB_TABLES tables(){
        RECVX_CB_TABLES t{};t.wpn=reinterpret_cast<const RECVX_CB_WPN_TAB*>(WpnTab);
        t.eff=reinterpret_cast<RECVX_CB_EF_WORK(*)[4]>(WpnEffTab);t.mtn_wpn=PlMtnWpn;t.knf_atr=KnfAtrTab;
        t.dam_near=EneDamNear;t.dam_mid=EneDamMid;t.dam_far=EneDamFar;t.ang=RECVX_CB_WPN_ANG_TAB;return t;}
    // Zombie (id 1) mit aufrechter watr-Kapsel: Kopf owP[10] oben, Fuesse unten (en01.c:3127-3142).
    void zombie(float x,float z,uint32_t flg,int hp){RECVX_ENEMY_WORK w{};w.id=1;w.flg=flg;w.hp=hp;w.px=x;w.pz=z;
        w.watr.c1={x,15.0f,z};w.watr.c2={x,1.4f,z};w.watr.r=2.0f;works.push_back(w);}
    // Liegender Zombie: dieselbe Kapsel flach am Boden (MV04, en01.c:5414).
    void lying(float x,float z,int hp){RECVX_ENEMY_WORK w{};w.id=1;w.flg=0x40979u;w.hp=hp;w.px=x;w.pz=z;
        w.watr.c1={x-7.0f,1.7f,z};w.watr.c2={x+7.0f,1.1f,z};w.watr.r=2.0f;works.push_back(w);}
    void bind(){table=RECVX_ENEMY_TABLE{};for(auto& w:works)table.slot[table.ewk_n++]=&w;}
    int nearest(float px,float pz,int ay){int32_t r=0,id=-1;float h=15.12f;
        return recvx_combat_search_near_enemy(&(*this).services_,nullptr,px,0,pz,ay,&r,&h,&id)?id:-1;}
    RECVX_COMBAT_SERVICES services_{};
    // Waagerechter Schuss der Waffe wno aus (0,0,0) nach -z (ay 0), wie player.c:5340-5386.
    void shoot(int wno){const auto t=tables();RECVX_CB_GA g{};const auto& wt=WpnTab[wno];
        g.wpn_no=wno;g.r=wt.r;g.l=wt.l;g.rn=wt.rn;g.rmax=wt.rmax;g.ax=0;g.ay=0;
        g.gx=0;g.gy=12;g.gz=-2;g.px=0;g.py=11;g.pz=-(3.0f+g.r);g.vx=0;g.vy=0;g.vz=-g.l;
        recvx_combat_check_gun_atari(&t,&services_,nullptr,&g);}
};

void rules(){
    _Make_SinTable();
    {   // Toter Zombie naeher als ein lebender: Aufschaltung auf den lebenden (pwksub.c:244).
        Rules r;r.zombie(0,-10,0x853u,-12);r.zombie(0,-30,0x979u,60);r.bind();r.services_=r.services();
        check(r.nearest(0,0,0)==1,"bhSearchNearEnemy schaltet einen toten Zombie (flg 2, ohne 0x20) auf");
        r.works[0].flg=0x40979u;
        check(r.nearest(0,0,0)==0,"Sterbefall vor DD (flg 0x20, ohne 2) ist im Original noch Ziel (en01.c:9961)");
    }
    for(int wno:{4,6}){ // M93R (Einzeltreffer) und Schrotflinte (WpnTab flg 0x80000000, Durchschlag)
        Rules r;r.zombie(0,-15,0x853u,-12);r.zombie(0,-35,0x979u,60);r.bind();r.services_=r.services();
        r.shoot(wno);
        check(!(r.works[0].flg&0x4u)&&r.works[0].dam[0]==0&&!r.works[0].comb_flg,"bhCheckGunAtari trifft einen toten Zombie (weapon.c:614)");
        check((r.works[1].flg&0x4u)&&r.works[1].dam[r.works[1].djnt_no]>0,"der tote Zombie verdeckt den lebenden dahinter");
    }
    for(int wno:{4,6}){ // Liegender (lebender) Zombie vor einem stehenden.
        Rules r;r.lying(0,-12,30);r.zombie(0,-35,0x979u,60);r.bind();r.services_=r.services();
        check(r.nearest(0,0,0)==0,"liegender lebender Zombie ist im Original naechstes Ziel");
        r.shoot(wno);
        check(!(r.works[0].flg&0x4u),"waagerechter Schuss beruehrt eine Bodenkapsel");
        check((r.works[1].flg&0x4u)&&r.works[1].dam[r.works[1].djnt_no]>0,"liegender Zombie verdeckt den stehenden");
    }
}

// Teil 2: bhCheckC2WallN-Bindung des Raumbesitzers an einer echten RM_0080-Wand.
void room_c2_wall(const std::filesystem::path& data){
    const auto bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);
    const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0080");
    const auto services=world.effect_combat_services();
    check(services.c2_wall_n!=nullptr,"bhCheckC2WallN des Effekt-Kampfpfads nicht gebunden");
    const BinaryView v(bytes);const auto header=inspect_room(bytes).room_offset;
    const auto base=v.u32(header+0x18),count=v.u32(header+0x98);
    unsigned found=0;
    for(unsigned i=0;i<count&&!found;++i){const auto p=base+36*i;
        const unsigned flags=v.u8(p),type=v.u8(p+1),attr=v.u32(p+4);
        const float px=v.f32(p+8),py=v.f32(p+12),pz=v.f32(p+16),w=v.f32(p+20),h=v.f32(p+24),d=v.f32(p+28);
        if(!(flags&1)||type!=0||(attr&0x2000000u)||w<=1||d<=4||h<=4)continue;
        // Sichtlinie parallel zur -x-Seite, 0.05 ausserhalb, halbe Hoehe (cap.r 0.1).
        auto line=[&](float gap){return RECVX_CB_CAPSULE{{px-gap,py+0.5f*h,pz+0.3f*d},{px-gap,py+0.5f*h,pz+0.7f*d},0.1f};};
        const auto graze=line(0.05f),clear=line(0.2f);
        NJS_POINT3 dir{0,0,1};const float length=0.4f*d;
        if(world.combat_wall_distance({graze.c1.x,graze.c1.y,graze.c1.z},dir,length,0x400)<length)continue; // Strahl muss frei sein
        if(world.c2_wall_n(clear,0x400)||world.c2_wall_n(clear,0x40000400))continue;                      // kein Nachbarsatz
        check(check_c2_wall_n({CollisionWall{flags,type,v.u8(p+2),int(std::int8_t(v.u8(p+3))),attr,{px,py,pz},{w,h,d},{}}},0,graze,0x400)==1,
              "Fixture: Originalkapsel beruehrt die Wand nicht");
        check(world.c2_wall_n(graze,0x400)==1&&world.c2_wall_n(graze,0x40000400)==1,
              "RoomWorld::c2_wall_n verliert den Kapselradius 0.1 (pwksub.c:2172)");
        check(services.c2_wall_n(services.user,&graze,0x400)==1&&services.c2_wall_n(services.user,&graze,0x40000400)==1,
              "Kampfbindung bhCheckC2WallN ist ein Strahl statt der Kapsel (pwksub.c:2172)");
        check(services.c2_wall_n(services.user,&clear,0x400)==0,"Kampfbindung sperrt eine freie Sichtlinie");
        found=i+1;
    }
    check(found,"RM_0080 hat keinen Quader fuer die Radiusprobe");
}
}

int main(int argc,char** argv){try{
    rules();
    if(argc>=2)room_c2_wall(argv[1]);
    std::cout<<"combat target rules: dead excluded (pwksub.c:244, weapon.c:614), dying/lying original, C2WallN capsule\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
