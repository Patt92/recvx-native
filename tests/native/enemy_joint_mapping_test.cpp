// owP-/obj_num-Abbildung der Gegnerdienste fuer Works mit Gesichtsmaske, am echten Raum RM_2050
// (ene0 id 91: 27 Modellknoten, Maske, mdl[1] mit 31 Objekten; Raumeffekt eff6 lkflg 2 lkono 28).
//
// Original: room.c:445-449 ruft fuer id > 90 bhInitMask (face.c:7). fmCnkInitContext haengt
// Zahn, Zunge und gueltige Augen als Tochterobjekte ein und zaehlt je mlwP->obj_num++
// (face_bh.c:535-561); _fmSetObjP (face_bh.c:67) legt objP und owP in Baumreihenfolge neu an.
// bhCalcTree (MdlPut.c:268) laeuft ueber diese obj_num Objekte: owP[n] ist der logische
// Knoten n. Leser mit diesem Indexraum: bhEne_InitDamage (zonzon1.c:806, dam[0..obj_num)),
// bhEne_AttackHitCheck (zonzon1.c:611), bhCheckBombAtari (weapon.c:1459),
// bhEff_E11_CheckCollisionPlayer (effsub2.c:2296: mdl[1].objP[i] gegen owP[i], i < obj_num)
// und bhSetEffectLink (room.c:853: lkono >= obj_num -> 0, nach bhInitMask :449 und vor
// bhSetEffectTable :469).
//
// Unabhaengiger Datenanker: mdl[1] (das Treffermodell aus den Raumdaten) ist in genau dieser
// logischen Reihenfolge angelegt; seine Objekte 15..22 liegen an den logischen Arm-/Schulter-
// knoten, nicht an den gleich nummerierten Modellknoten.
// Testaufstellung: Raumaufbau ohne Spieler und ohne Tick (owP aus room.c:438-444 bhCalcModel);
// dam[29] wird als Vorbedingung beschrieben, die Strahlen sind Prueflinien, kein Spielablauf.
// Gegenproben (je einzeln, Test ROT): s.obj_num wieder bones() -> "obj_num"; s.joint_matrix
// ohne owp_node -> "joint_matrix"; Raumeffekt-Klemmung wieder bones() -> "lkono 28";
// effect_player_hit wieder physisches owP/bones() -> "CheckCollisionPlayer".
#include "world.h"
#include "room.h"
#include "hit_model.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "enemy_work.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
namespace recvx {
// Nur dieser Test; ruft den vorhandenen Raumbesitzer, keine neue Matrix.
struct EnemyJointMappingTestAccess {
    static int player_hit(RoomWorld& w,NJS_POINT3 p1,NJS_POINT3& p2){return w.effect_player_hit(p1,p2);}
};
}
struct Room {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=0;std::int64_t frame=0;AudioMixer audio;RoomWorld world;
    Room(const std::filesystem::path& data,const std::string& name):bytes(RoomArchive(data).load(name+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),
        world(bytes,scene,actor,motion,frame,audio,data,{},name){}
};
static unsigned logical_count(const PlayerAsset& a){unsigned n=0;for(;;++n){try{(void)a.model_index(n);}catch(const std::runtime_error&){return n;}}}
static unsigned slot_of(RoomWorld& world,const RECVX_ENEMY_WORK* w){
    auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]==w)return i;
    throw std::runtime_error("work not in sys->ene");
}
static float dist(NJS_POINT3 a,NJS_POINT3 b){const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();
    Room r(argv[1],"RM_2050");auto& world=r.world;
    const auto* e=world.state(1,0);check(e&&e->enemy_bound&&e->enemy_native&&e->enemy.id==91,"RM_2050 ene0 is not the id 91 subplayer");
    const auto* asset=e->enemy_asset;check(asset&&asset->face(),"id 91 model without MASK");
    const auto* hit=asset->model(1);check(hit!=nullptr,"id 91 mdl[1] hit model missing");
    const unsigned logical=logical_count(*asset);
    // Datenlage: 27 Modellknoten, vier eingefuegte Maskenobjekte; mdl[1] hat obj_num 31 Objekte.
    check(asset->bones()==27&&logical==31&&hit->bones()==31,"unexpected id 91 node counts");
    check(asset->model_index(8)==7,"first inserted mask child is not logical node 8");
    // Unabhaengiger Anker: mdl[1].objP[i] liegt am logischen Knoten i, nicht am Modellknoten i.
    for(unsigned i=15;i<=22;++i){
        const auto h=hit->bind_position(i),l=asset->bind_position(asset->model_index(i)),p=asset->bind_position(i);
        check(dist(h,l)<0.35f&&dist(h,p)>dist(h,l)+0.3f,"mdl[1] object "+std::to_string(i)+" is not in logical owP order");
    }
    auto* w=const_cast<RECVX_ENEMY_WORK*>(&e->enemy);const auto* sv=w->sv;
    check(sv&&sv->obj_num&&sv->joint_matrix&&sv->joint_point,"enemy services not bound to the work");
    const unsigned slot=slot_of(world,w);
    // mlwP->obj_num nach bhInitMask.
    check(sv->obj_num(w)==31,"obj_num: mlwP->obj_num must count the inserted mask objects (31)");
    // owP[n] fuer alle logischen Knoten; Gegenstueck ist der bereits logische Leser
    // pwksub.c:455 epp->mlwP->owP[node] (enemy_slot_joint_matrix).
    bool differs_from_physical=false;
    for(unsigned n=0;n<31;++n){
        float m[16];check(sv->joint_matrix(w,n,m)==1,"joint_matrix: owP["+std::to_string(n)+"] unavailable");
        const auto ref=world.enemy_slot_joint_matrix(slot,n);check(ref.has_value(),"enemy_slot_joint_matrix unavailable");
        check(std::memcmp(m,ref->value,sizeof m)==0,"joint_matrix: owP["+std::to_string(n)+"] is not the logical node");
        if(n<27&&std::memcmp(m,e->enemy_owp_mtx[n].value,sizeof m)!=0)differs_from_physical=true;
        const RECVX_ENEMY_VEC local{0.5f,-1.0f,2.0f};RECVX_ENEMY_VEC out{};check(sv->joint_point(w,n,&local,&out)==1,"joint_point unavailable");
        NJS_MATRIX mm;std::memcpy(mm,ref->value,sizeof mm);NJS_POINT3 a{0.5f,-1.0f,2.0f},b;njCalcPoint(&mm,&a,&b);
        check(out.x==b.x&&out.y==b.y&&out.z==b.z,"joint_point: owP["+std::to_string(n)+"] is not the logical node");
    }
    check(differs_from_physical,"test cannot tell logical from physical owP");
    float none[16];check(sv->joint_matrix(w,31,none)==0,"joint_matrix: owP[obj_num] must be outside");
    // zonzon1.c:806 bhEne_InitDamage loescht dam[0..obj_num).
    w->dam[29]=5;w->dam[31]=7;recvx_enemy_init_damage(w);
    check(w->dam[29]==0,"obj_num: bhEne_InitDamage must clear dam[29] of the masked work");
    check(w->dam[31]==7,"bhEne_InitDamage cleared past obj_num");w->dam[31]=0;
    // room.c:853 bhSetEffectLink: lkono 28 < obj_num 31 bleibt; lkono 3/18 unveraendert.
    {const auto* s6=world.effect_state(6);check(s6&&(s6->flg&0x80u)&&s6->link_kind==1&&s6->link_index==0,"RM_2050 eff6 not linked to ene0");
     check(s6->lkono==28,"lkono 28: bhSetEffectLink clamps against the masked obj_num 31");
     const auto* s4=world.effect_state(4);const auto* s5=world.effect_state(5);check(s4&&s5&&s4->lkono==18&&s5->lkono==3,"other ene0 links changed");}
    // effsub2.c:2296 bhEff_E11_CheckCollisionPlayer: mdl[1].objP[i] unter owP[i] fuer i < obj_num.
    // Referenzschleife des Originals mit den logischen owP aus enemy_slot_joint_matrix.
    {const auto objects=decode_hit_model(hit->encoded_model());check(objects.size()==31,"mdl[1] object table");
     auto reference=[&](NJS_POINT3 p1,NJS_POINT3& p2){int ret=-1;for(unsigned i=0;i<31;++i){if(!objects[i])continue;const auto m=world.enemy_slot_joint_matrix(slot,i);
         const auto res=line_hit_model(p1,p2,*objects[i],m->value);check(res!=HitModelResult::IndexOutside,"hit model index");if(res==HitModelResult::Hit)ret=int(i);}return ret;};
     bool tested=false;
     for(unsigned k=27;k<31&&!tested;++k){if(!objects[k]||objects[k]->vertices.empty())continue;
         const auto m=world.enemy_slot_joint_matrix(slot,k);NJS_MATRIX mm;std::memcpy(mm,m->value,sizeof mm);NJS_POINT3 c=objects[k]->center,wc;njCalcPoint(&mm,&c,&wc);
         const NJS_POINT3 p1{wc.x,wc.y,wc.z+50};NJS_POINT3 want{wc.x,wc.y,wc.z-50},got=want;
         const int expected=reference(p1,want);if(expected<27)continue;
         const int actual=EnemyJointMappingTestAccess::player_hit(world,p1,got);
         check(actual==expected,"CheckCollisionPlayer: object index "+std::to_string(actual)+" instead of "+std::to_string(expected));
         check(got.x==want.x&&got.y==want.y&&got.z==want.z,"CheckCollisionPlayer: p2 not shortened like the original loop");
         tested=true;}
     check(tested,"no mdl[1] object 27..30 crossed by its centre line");}
    // Ohne Maske bleibt die Abbildung die Identitaet (RM_5810 EN13, id <= 90).
    {Room q(argv[1],"RM_5810");const auto* f=q.world.state(1,0);check(f&&f->enemy_asset&&!f->enemy_asset->face(),"RM_5810 ene0");
     const auto* fw=&f->enemy;check(fw->sv->obj_num(fw)==f->enemy_asset->bones(),"unmasked obj_num changed");
     for(unsigned n=0;n<f->enemy_asset->bones();++n){float m[16];check(fw->sv->joint_matrix(fw,n,m)==1&&std::memcmp(m,f->enemy_owp_mtx[n].value,sizeof m)==0,"unmasked owP changed");}}
    std::cout<<"enemy joint mapping tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
