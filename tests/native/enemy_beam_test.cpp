// effsub2.c:2014/2092 bhEff_E11_SearchLight(Draw), :2353/2390 bhEff_E16_LaserSight(Draw),
// :2296 bhEff_E11_CheckCollisionPlayer and hitchkl.c:735 bhCollisionCheckL2MDL on the
// player's original hit model (mdl[3]). The link/player providers below are test
// providers with stated values, not a played room.
#include "effect_sprites.h"
#include "effect_sprites_internal.h"
#include "hit_model.h"
#include "actor.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "world.h"
#include "room.h"
#include "enemies/en16.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool near(float a,float b,float eps=1e-3f){return std::fabs(a-b)<=eps;}

// --- EN16-Schussfolge in echten Raeumen (en16.c:211-277 bhEne16_MV02 mode3 4) ----------
// RM_3010 traegt den Scharfschuetzen (EN16 type 1, Init mode1 1/mode2 2, en16.c:63-69).
// Diagnose-Vorbedingungen, kein natuerliches Durchspielen: (1) die Raumfigur ist beim Laden
// per Skript zurueckgehalten (stflg 0x1000000, bhEneSetCk event.c:2847); die Freigabe durch
// das Raumereignis ist hier durch Loeschen des Bits ersetzt. (2) Die Spielerin steht auf
// Boden 0 bei (80, 0, 40) in einer type-2-Flaeche mit prm0 16 (bhEne16_BR00 en16.c:110-117).
// (3) Fuer den Wandzweig wird plp->stflg 0x10 gesetzt, damit en16.c:217 `!(plp->stflg & 0x30)`
// den Spielerzweig ausschliesst. Alle Erwartungen stammen aus den Originalzeilen.
struct BeamRoom {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    std::vector<BoneMatrix> nodes;
    BeamRoom(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);
        world.set_enemy_ai(true);world.set_enemy_grab(true);world.set_camera_offset_owner(&camera);
        commit();events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    // player.c:840/1688 bhCalcModel(plp): owP des Spielers vor jedem Pass (runtime.cpp).
    void commit(){nodes=player.script_nodes(player.pose(motion,frame,actor.position,actor.yaw));world.commit_player_joints(nodes);}
    void tick(){commit();world.set_simulation_flags(events.flags[6][0]);world.tick();}
    RECVX_ENEMY_WORK* en16(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==16)return t.slot[i];return nullptr;}
    const EffectSprite& laser(RECVX_ENEMY_WORK* w){std::int32_t s=0;std::memcpy(&s,w->exp0,4);check(s>0,"EN16 EXP0_I(0) ohne eff");
        const auto* e=world.effect_state(unsigned(s-1));check(e&&e->id==270,"EN16 eff[EXP0_I(0)] ist nicht 270");return *e;}
    std::set<unsigned> live(unsigned id){std::set<unsigned> r;for(unsigned k=0;k<512;++k){const auto* x=world.effect_state(k);if(x&&(x->flg&3u)==1u&&x->id==id)r.insert(k);}return r;}
    // Bis zum Pass, in dem mode3 4 schiesst: case 3 erhoeht mode3 bei ct0-- == 0 (en16.c:205-210).
    void until_shot(RECVX_ENEMY_WORK* w){for(int i=0;i<600&&!(w->mode2==2&&w->mode3==4);++i)tick();check(w->mode2==2&&w->mode3==4,"EN16 erreicht MV02 mode3 4 nicht");}
};
static unsigned new_one(const std::set<unsigned>& before,const std::set<unsigned>& after,const char* what){
    unsigned found=0,count=0;for(unsigned k:after)if(!before.count(k)){found=k;++count;}check(count==1,what);return found;}
static void en16_room_shots(const std::filesystem::path& data){
    {   // RM_1020: Alfred als type 0 (en16.c:57-62 mode2 1 = MV01, kein Brain); der Strahl ist an
        // (eff mode0 2, en16.c:145-151), die Schussfolge MV02 ist dort ohne Ereignis nicht erreichbar.
        BeamRoom r(data,"RM_1020");for(int i=0;i<3;++i)r.tick();auto* w=r.en16();
        check(w&&w->type==0&&w->mode1==0&&w->mode2==1,"RM_1020: EN16 type 0 in MV01 erwartet");
        check(r.laser(w).mode0==2,"RM_1020: MV01 muss den Strahl auf mode0 2 halten");
    }
    BeamRoom r(data,"RM_3010");r.tick();auto* w=r.en16();
    check(w&&w->type==1&&(w->stflg&0x1000000u),"RM_3010: zurueckgehaltener EN16 type 1 fehlt");
    w->stflg&=~0x1000000u;                                         // Diagnose (1)
    r.actor.position={80,0,40};
    {RECVX_ENEMY_PLAYER p;check(r.world.enemy_player_get(p),"player_get");p.pos={80,0,40};p.flr_no=0;check(r.world.enemy_player_set(p),"player_set");} // Diagnose (2)
    // --- Spielertreffer en16.c:217-248 ---------------------------------------------------
    r.until_shot(w);
    const auto& eff=r.laser(w);
    check(eff.ct0>0,"Aufstellung: der Strahl liegt beim Schusspass nicht auf einem Spielergelenk > 0");
    const int jno=eff.ct0;const NJS_POINT3 gp{eff.gpx,eff.gpy,eff.gpz};
    const NJS_POINT3 dv{w->xn-w->px,w->yn-w->py,w->zn-w->pz};      // :219-221 plp->dv
    RECVX_ENEMY_PLAYER before;check(r.world.enemy_player_get(before),"player_get");
    const int ct1=w->ct1;const auto blood_before=r.live(297);
    r.commit();r.world.set_simulation_flags(r.events.flags[6][0]);r.world.tick_enemy_pass();
    RECVX_ENEMY_PLAYER after;check(r.world.enemy_player_get(after),"player_get");
    // :224-236
    const int hp=before.hp>150?before.hp-before.hp/2:before.hp>50?before.hp-before.hp/3:-1;
    check(after.hp==hp&&r.inv.hp==hp,"plp->hp folgt nicht en16.c:224-236");
    check(hp>=0,"Aufstellung: erster Treffer darf nicht toeten");
    // :237-243 flg |= 4, mode0 2, mode1 = bhEne_DGDirCheck(plp) (zonzon1.c:495/515), mode2 1, mode3 0
    NJS_VECTOR dir{dv.x,dv.y,dv.z},rot;njUnitMatrix(nullptr);njRotateY(nullptr,-before.ay);njCalcVector(nullptr,&dir,&rot);
    check((after.flg&4u)&&after.mode0==2&&after.mode1==(rot.z>=0?0:1)&&after.mode2==1&&after.mode3==0,"Spielerzustand nach Treffer weicht von en16.c:237-243 ab");
    check(w->ct1==ct1-1&&w->mode3==5&&w->ct0==0,"EN16 ct1/mode3 nach dem Schuss");
    // :223 bhEne_SetBloodEffectBurst(plp, 2, eff->ct0, &eff->gpx, 1) -> zonzon1.c:243-298
    const unsigned b=new_one(blood_before,r.live(297),"bhEne_SetBloodEffectBurst muss genau einen eff 297 anlegen");
    const auto& blood=*r.world.effect_state(b);
    check(blood.type==2&&blood.lkono==jno&&blood.link_kind==0&&(blood.flg&0x80u),"Blut 297: type 2 am Spielergelenk eff->ct0 (plp-Link)");
    check(blood.mdlver==0,"Blut 297: mdlver = BloodType[plp->id = 0].color = 0 (zonzon1.c:11, player.c:700)");
    {   // zonzon1.c:258-266: ofp = gp - owP[jno].t, durch die Inverse von owP[jno].mtx gedreht
        NJS_MATRIX m;std::memcpy(m,r.nodes.at(std::size_t(jno)).value,sizeof m);NJS_VECTOR o{gp.x-m[12],gp.y-m[13],gp.z-m[14]},l;
        njInvertMatrix(&m);njCalcVector(&m,&o,&l);
        check(near(blood.lox,l.x)&&near(blood.loy,l.y)&&near(blood.loz,l.z),"Blut 297: lox/loy/loz nicht der Gelenkversatz des Strahlendpunkts");
    }
    check(near(blood.xn,-dv.x)&&near(blood.yn,-dv.y)&&near(blood.zn,-dv.z),"Blut 297: xn/yn/zn != -plp->dv (zonzon1.c:292-294)");
    r.world.tick_after_player();
    // --- Wandtreffer en16.c:250-266 -------------------------------------------------------
    r.until_shot(w);
    {RECVX_ENEMY_PLAYER p;check(r.world.enemy_player_get(p),"player_get");p.stflg|=0x10u;check(r.world.enemy_player_set(p),"player_set");} // Diagnose (3)
    const auto& eff2=r.laser(w);
    check(eff2.line_wall.has_value(),"Aufstellung: Strahl endet beim Schusspass an keiner Flaeche (eff->exp0 NULL)");
    const auto& hit=*eff2.line_wall;
    // hitchk.c:5881-5905: attr 0x100 aus -> 1 (diese Aufstellung trifft eine solche Flaeche).
    check(!(hit.attributes&0x100u),"Aufstellung: Wandattribut ohne 0x100 erwartet");
    const NJS_POINT3 lo{eff2.lox,eff2.loy,eff2.loz},v{w->px-w->xn,w->py-w->yn,w->pz-w->zn};
    const int hp2=r.inv.hp;const auto spark_before=r.live(308),s11=r.live(11),s10=r.live(10);
    r.commit();r.world.set_simulation_flags(r.events.flags[6][0]);r.world.tick_enemy_pass();
    check(r.inv.hp==hp2,"Wandzweig darf keinen Schaden machen");
    // :262 bhSetEffGunSpark(&lo, &v, 0xFFF8F8A0, 0x4CE04010, WpnTab[13].hiteff - 4 = 1),
    // effsub3.c:2401-2427 Eff308Prm[1] = {10, 0.6, 0.98, 2, 7, 50, 0.8, 4, -0.0326667}.
    const unsigned sp=new_one(spark_before,r.live(308),"bhSetEffGunSpark muss genau einen eff 308 anlegen");
    const auto& spark=*r.world.effect_state(sp)->spark308;
    check(near(spark.vtx_pos.x,lo.x)&&near(spark.vtx_pos.y,lo.y)&&near(spark.vtx_pos.z,lo.z),"Funken: vtx_pos != eff->lox/loy/loz");
    check(near(spark.vtx_dir.x,v.x)&&near(spark.vtx_dir.y,v.y)&&near(spark.vtx_dir.z,v.z),"Funken: vtx_dir != epw->p - epw->n");
    check(spark.col_src==0xFFF8F8A0u&&spark.col_dst==0x4CE04010u&&near(spark.gnd_hgh,lo.y-100.0f),"Funken: Farben/gnd_hgh");
    check(spark.prm_a.vtx_num==10&&near(spark.prm_a.speed,0.6f,1e-6f)&&spark.prm_a.time==7&&near(spark.prm_a.ang_rand,50.0f,1e-6f),"Funken: nicht Eff308Prm[WpnTab[13].hiteff - 4 = 1]");
    // :263-265 bhSetGunSplash liest den nie beschriebenen GA_WORK vx/vy/vz: kein Spritzer, benannte Luecke.
    check(r.live(11)==s11&&r.live(10)==s10,"bhSetGunSplash darf ohne belegte Richtung keinen Spritzer erzeugen");
    check(r.world.unbound_enemy_routines().count("bhSetGunSplash (en16.c:265): GA_WORK vx/vy/vz im Original uninitialisiert")==1,"Luecke bhSetGunSplash nicht gemeldet");
    r.world.tick_after_player();
    for(const auto& g:r.world.unbound_enemy_routines())
        if(g.rfind("bhSetGunSplash (en16.c:265)",0)!=0){std::cerr<<"unerwartete Luecke: "<<g<<'\n';throw std::runtime_error("EN16-Schussfolge meldet eine weitere Luecke");}
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();
    // --- player hit model -----------------------------------------------------
    {
        const auto player=PlayerAsset::load(argv[1],10);
        const auto* hit=player.model(3);check(hit!=nullptr,"plp->mdl[3] hit model missing");
        const auto objects=decode_hit_model(hit->encoded_model());
        check(objects.size()==player.bones(),"mdl[3] object count differs from plp->mlwP->obj_num");
        const auto nodes=player.pose(0,0);
        unsigned tested=0;
        for(std::size_t i=0;i<objects.size()&&i<nodes.size();++i){
            if(!objects[i]||objects[i]->vertices.empty())continue;
            NJS_MATRIX m;std::copy(nodes[i].value,nodes[i].value+16,m);
            NJS_POINT3 local=objects[i]->center,center;njCalcPoint(&m,&local,&center);
            // A segment through the bounding centre along z from both sides, and one far beside it.
            NJS_POINT3 end{center.x,center.y,center.z-50};
            const auto front=line_hit_model({center.x,center.y,center.z+50},end,*objects[i],nodes[i].value);
            check(front!=HitModelResult::IndexOutside,"hit model polygon index outside its point list");
            if(front==HitModelResult::Hit){check(end.z>center.z-50&&end.z<center.z+50,"L2PL hit did not shorten p2 onto the mesh");++tested;}
            NJS_POINT3 far_end{center.x+objects[i]->radius+10,center.y,center.z-50};
            check(line_hit_model({center.x+objects[i]->radius+10,center.y,center.z+50},far_end,*objects[i],nodes[i].value)==HitModelResult::Miss,"line outside mdl->r reported a hit");
        }
        check(tested>0,"no hit-model object is crossed by a line through its centre");
    }
    // --- shared test providers -----------------------------------------------
    NJS_MATRIX link;njUnitMatrix(&link);link[12]=1;link[13]=10;link[14]=2;   // node faces -z
    set_effect_link_lookup([&](unsigned kind,unsigned,unsigned,NJS_MATRIX& out){if(kind!=1)return false;std::copy(link,link+16,out);return true;});
    int player_index=-1;NJS_POINT3 player_point{1,10,-20};
    set_effect_player_hit([&](NJS_POINT3,NJS_POINT3& p2){if(player_index>=0)p2=player_point;return player_index;});
    static const std::vector<CollisionWall> no_walls;set_effect_walls(&no_walls,0);
    int next_extra=100;set_effect_extra_allocator([&](){return next_extra++;},[](int){});
    EffectContext context;context.sp_flg=8;
    // --- 270 laser sight -------------------------------------------------------
    {
        RECVX_ENEMY_WORK owner{};owner.flg=1;owner.type=1;owner.owner_index=0;
        unsigned char block[0x68]{};owner.exp0=block;
        EffectSpawn spawn;spawn.id=270;spawn.flg=1;spawn.scale={0,0,0};spawn.linked_work=&owner;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        bool queued=true;step_effect_sprite(s,context,&queued);
        check(!queued&&s.mode0==1,"bhEff_E16_LaserSight case 0 must only arm func and mode0 1");
        step_effect_sprite(s,context,&queued);check(!queued&&s.mode0==1,"mode0 1 must stay idle");
        s.mode0=2;player_index=3;step_effect_sprite(s,context,&queued);
        check(queued&&s.ct0==3,"laser draw must store the CheckCollisionPlayer object index in ct0");
        check(s.gpx==player_point.x&&s.gpz==player_point.z,"laser gp must be the shortened player hit point");
        check(recvx_en16_eff_num(&owner)==1,"bhEne16_AddEffPos not called once per draw");
        check(s.quads.size()==16&&s.lines.size()==2,"laser draw: 16 cone polygons, main line plus one trail line (type != 0)");
        for(const auto& q:s.quads)check(q.pn==3&&q.gouraud&&!q.textured&&!q.fogged&&q.source==8&&q.destination==10,"laser polygon state differs from njDrawPolygon3D after 8/10");
        // The apex is black with alpha 0, the rim red 100 with 255*rate.
        check((s.quads[0].vertices[0].col>>24)==0&&((s.quads[0].vertices[1].col>>16)&255u)==100,"laser polygon colours differ");
        owner.stflg|=0x1000000u;step_effect_sprite(s,context,&queued);check(!queued,"stflg 0x1000000 must suppress the ef_fnc entry");
        owner.flg=0;step_effect_sprite(s,context,&queued);check(!s.flg,"laser must release when lkwkp->flg is 0");
    }
    // --- 261 search light ------------------------------------------------------
    {
        RECVX_ENEMY_WORK owner{};owner.flg=1;owner.owner_index=0;
        EffectSpawn spawn;spawn.id=261;spawn.flg=1;spawn.scale={0,0,0};spawn.linked_work=&owner;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        bool queued=true;step_effect_sprite(s,context,&queued);
        check(!queued&&s.mode0==0xFF&&s.extra&&s.extra_slot==100,"case 0: mode0 0xFF with bhSetExtraEffectWork");
        s.mode0=1;s.axp=0x38e;player_index=-1;step_effect_sprite(s,context,&queued);
        check(queued&&s.ct0==1&&s.ax==0x38e/16&&s.ct1==10&&s.ct2==0,"case 1 ramp (ax += (short)(axp-ax)/16, ct1 += 10)");
        check(s.quads.size()==16&&s.lines.size()==2,"search light: 16 fan polygons and two sweep lines");
        s.mode0=2;player_index=0;step_effect_sprite(s,context,&queued);
        check(s.ct2==1&&s.ax==0x38e&&s.ct0==32,"player in the cone in mode0 2 must set ct2 (alarm)");
        s.mode0=3;s.ct0=3;step_effect_sprite(s,context,&queued);check(s.mode0==0&&s.ct0==0,"case 3 fade must return to mode0 0");
        // Gouraud alpha per vertex survives into the scene triangle.
        Scene scene;SceneCamera camera;camera.perspective=0x2000;
        EffectSprite triangle;triangle.id=302;triangle.pn=3;triangle.gouraud=true;triangle.untextured=true;triangle.bl_src=8;triangle.bl_dst=10;
        triangle.tv[0]={0,0,0,0,0,0xff102030u};triangle.tv[1]={1,0,0,0,0,0x00102030u};triangle.tv[2]={0,1,0,0,0,0x80102030u};
        append_effect_sprite(scene,triangle,{0,0,0},{1,1,1},camera,0);
        check(scene.triangles.size()==1&&scene.triangles[0].vertex_alpha,"per-vertex effect alpha not carried into the scene");
        check(std::fabs(scene.triangles[0].vertices[1].alpha)<1e-6f&&std::fabs(scene.triangles[0].vertices[2].alpha-128.f/255.f)<1e-6f,"vertex alpha values differ");
    }
    en16_room_shots(argv[1]);
    std::cout<<"hit model L2MDL, laser sight 270, search light 261, per-vertex effect alpha and EN16 shots (RM_1020/RM_3010) PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
