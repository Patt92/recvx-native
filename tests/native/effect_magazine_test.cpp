// effsub1.c:2335 bhEff007 — das beim Nachladen ausgeworfene Magazin (pwksub.c:3372
// bhSetMagazine). Geprueft auf den Effektdiensten ohne Raum: Start am Handgelenk,
// Fall mit Schwerkraft, Aufprall mit Rueckprall und CallPlayerWeaponSeEx(259),
// Drehung in Fall 2, Ende eine halbe Einheit tiefer, Wasserbremse mit Spritzer 15
// und den Schreibauftrag auf das rechte Waffenmodell (sys->obwp).
#include "effect_sprites.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include "combat.h"
#include "room.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace recvx;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct WeaponSe {int calls=0,se=0,slot=-1;NJS_POINT3 at{};};
// bhSetMagazine (pwksub.c:3372): id 7, flg 0x40001, type 0, flr_no = sys->mg_ct,
// ay = ang + 16384, Skalierung 1; mdlver 0/1/2 nach Waffe. Der Satz haengt am
// Handgelenk (bhSetEffectTb mit lkp = plp), der Raumbesitzer setzt px vor dem Pass.
static EffectSprite magazine(unsigned mdlver,NJS_POINT3 pos){
    EffectSpawn spawn;spawn.flg=0x40001;spawn.id=7;spawn.type=0;spawn.mdlver=mdlver;spawn.flr_no=0;
    spawn.position=pos;spawn.scale={1,1,1};spawn.ay=16384;
    EffectSprite s;initialize_effect_sprite(s,spawn);
    s.flg|=0x80u;                       // bhSetEffectTb mit Linkzeiger
    return s;
}
static EffectContext ctx{};
// effect.c:794: pxb = px vor dem Prozeduraufruf.
static bool step(EffectSprite& s){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;bool q=false;step_effect_sprite(s,ctx,&q);return q;}
static std::size_t part2(const Scene& s){std::size_t n=0;for(const auto& t:s.triangles)if(t.object==2)++n;return n;}
static void weapon_owner(const std::filesystem::path& data,unsigned weapon){
    const unsigned item=weapon==12?2:3,ammo=weapon==12?42:40;
    // Diagnostisches Inventar: leere Waffe 12/13 plus Reserve. Danach laeuft der echte
    // Reloadzustand durch CombatController -> bhSetMagazine -> RoomWorld -> bhEff007.
    auto bytes=RoomArchive(data).load("RM_1000.RDX",64*1024*1024);auto stage=decode_scene(bytes);
    auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    auto player=PlayerAsset::load(data);Inventory inv;MessageDialog dlg;EventVm ev(bytes,inv,dlg);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_1000");world.set_player_asset(&player);world.bind_player_collision(ev);
    CombatController combat(data);world.set_weapon_part2_hook([&]{combat.set_weapon_part2_hidden(true);});
    check(inv.acquire(item,0,0)&&inv.acquire(ammo,weapon==12?100:7,0),"Waffe/Reserve nicht aufgenommen");
    unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(inv.slots()[i])==item)slot=i;
    check(slot&&inv.equip(slot)&&inv.weapon()==weapon,"Waffe nicht ausgeruestet");
    const auto nodes=[&]{return player.pose(motion,frame,actor.position,actor.yaw);};
    const auto draw=[&]{Scene s;combat.append(s,nodes());return s;};
    bool hidden=false,shown=false;std::size_t full=0,magazine_triangles=0;
    for(unsigned pass=0;pass<250&&!shown;++pass){
        world.set_player_weapon(inv.weapon());world.set_simulation_flags(~0u);
        world.commit_player_joints(player.script_nodes(nodes()));
        combat.tick({true,pass>20,0,0,false},false,inv,player,actor,motion,frame,&world,audio,&ev);
        world.tick_after_player();const auto s=draw();
        if(!full){full=s.triangles.size();magazine_triangles=part2(s);check(magazine_triangles>0,"AK-47-Knoten 2 liefert keine Dreiecke");}
        bool ejected=false;for(unsigned i=0;const auto* e=world.effect_state(i);++i)if(e->id==7&&(e->flg&1u))ejected=true;
        if(!hidden&&ejected){check(part2(s)==0&&s.triangles.size()==full-magazine_triangles,"Auswurf versteckt rechtes Magazin nicht");hidden=true;}
        else if(hidden&&weapon==12&&part2(s)>0){check(s.triangles.size()==full,"Einsetzen stellt Waffenmodell nicht wieder her");shown=true;}
        if(hidden&&weapon==13)check(part2(s)==0,"Waffe 13 hat keinen Magazin-Clear-Schreiber");
    }
    if(weapon==13)shown=hidden; // player.c:6073: nur Waffe 12 setzt es beim Einsetzen zurueck.
    if(!hidden||!shown)std::cerr<<"reload hidden="<<hidden<<" shown="<<shown<<" mode="<<combat.original_mode()<<" phase="<<combat.original_phase()<<" motion="<<motion<<" frame="<<frame/65536<<" ammo="<<inv.slots()[slot]<<'\n';
    check(hidden&&shown,"Reload muss Magazin auswerfen und bei act_ct1 wieder einsetzen");
    // Ein verbrauchter Clear-Auftrag darf einen spaeteren Hide-Schreiber nicht mehr ueberstimmen.
    combat.set_weapon_part2_hidden(true);
    combat.tick({true,false,0,0,false},false,inv,player,actor,motion,frame,&world,audio,&ev);
    check(part2(draw())==0,"Verbrauchter Clear-Auftrag wird im naechsten Kampfpass erneut angewendet");
    combat.tick({},true,inv,player,actor,motion,frame,&world,audio,&ev);
    check(part2(draw())==0,"Abbruch des Kampfzustands setzt Modellflag zurueck");
    inv.equip(slot);combat.tick({},false,inv,player,actor,motion,frame,&world,audio,&ev);
    inv.equip(slot);combat.tick({},false,inv,player,actor,motion,frame,&world,audio,&ev);
    check(part2(draw())>0,"Erneutes Ressourcenladen muss die unveraenderte Modellvorlage liefern");
    for(const auto& g:world.unbound_enemy_routines())check(g.find("sys->obwp")==std::string::npos,"Waffenmodellbesitzer nicht gebunden");
    // Die rechte Auswahl darf weder Kinder noch das linke Modell mit demselben Objektindex filtern.
    WeaponAsset synthetic;SceneTriangle t;t.object=2;synthetic.right.triangles.push_back(t);t.object=3;synthetic.right.triangles.push_back(t);t.object=2;synthetic.left.triangles.push_back(t);
    synthetic.right.ambient_writes.push_back({0,{1,0,0},0,2,false});
    synthetic.right.ambient_writes.push_back({1,{0,1,0},0,3,false});
    synthetic.left.ambient_writes.push_back({0,{0,0,1},0,2,false});
    std::vector<BoneMatrix> bones(14);for(auto& b:bones)njUnitMatrix(&b.value);Scene kept;synthetic.append(kept,bones,true);
    check(kept.triangles.size()==2&&part2(kept)==1,"Objektflag darf nur rechtes Knotenmodell 2 unterdruecken");
    check(kept.ambient_writes.size()==2&&kept.ambient_writes[0].object==3&&kept.ambient_writes[0].triangle==0&&kept.ambient_writes[1].triangle==1,"Verdecktes Modell schreibt Materialzustand oder verschiebt nachfolgende Materialpositionen");
    kept.resolve_ambient_material();check(kept.triangles[0].ambient_material.y==1&&kept.triangles[1].ambient_material.z==1,"Materialweitergabe nach Knotenfilter falsch");
}
int main(int argc,char** argv){try{_Make_SinTable();
    check(supported_effect_sprite(7),"bhEff007 nicht als Handport registriert");
    // Boden: Kasten 0..20 x 0..10 y 0..20 z, Oberseite y 10.
    const std::vector<CollisionWall> walls{{1,0,0,0,0,{0,0,0},{20,10,20},{}}};
    set_effect_walls(&walls,30);
    GroundCollision ground;ground.update(walls,30);
    set_effect_ground(&ground,nullptr,nullptr);
    WeaponSe se;
    bool water=false;float water_top=12.0f;
    {EffectWorldHooks h;
     h.weapon_se=[&se](NJS_POINT3 p,int n,int slot){++se.calls;se.se=n;se.slot=slot;se.at=p;};
     set_effect_world_hooks(std::move(h));}
    set_effect_water([&](NJS_POINT3,float& top){if(!water)return false;top=water_top;return true;});
    ctx={0,1,0,0,0,0,8};                          // st_flg 0, pt_flg 1, sp_flg 8
    set_effect_context(ctx);
    take_effect_output();
    {
        // 1. Trockener Fall: Start, Schwerkraft, Aufprall, Drehung, Ende.
        EffectSprite s=magazine(0,{10,18,10});
        step(s);
        check(s.polygon_queued,"bhEff007 steht nicht in sys->ef_pol");
        check(s.tex_id==7&&s.ani_ct==0,"Start: tex_id 7, ani_ct 0");
        check(!(s.flg&0x80u),"Start: flg 0x80 geloescht (der Satz loest sich vom Gelenk)");
        check(std::fabs(s.gpy-10.6f)<1e-4f,"Start: gpy = 0.6 + bhGetGroundPosition");
        check(s.ct0==0&&s.mode0==1,"Start: ct0 0, mode0 1");
        // Der erste Pass faellt durch Fall 0 nach 1 und bewegt schon: py -= 0.3, danach yn -0.35.
        check(std::fabs(s.py-17.7f)<1e-4f&&std::fabs(s.yn-(-0.35f))<1e-6f,"Pass 1: py += yn (-0.3), danach yn -= 0.05");
        const auto first=take_effect_output();
        check(first.gaps.empty(),"mdlver 0 meldet keine Luecke");
        // Fall bis zum Aufprall auf gpy 10.6.
        int passes=0;float last_yn=s.yn;
        while(s.mode0==1&&s.flg&&passes<400){last_yn=s.yn;step(s);++passes;}
        check(s.mode0==2,"Aufprall erreicht mode0 2 nicht");
        check(std::fabs(s.py-10.6f)<1e-4f,"Aufprall: py = gpy");
        // Reihenfolge im Original: py += yn, dann die Schwerkraft, erst danach der Bodentest.
        const float fallen=last_yn>-1.0f?last_yn-0.05f:last_yn;
        check(std::fabs(s.yn-(0.3f*-fallen))<1e-5f,"Aufprall: yn = 0.3 * -yn (Rueckprall)");
        check(std::fabs(s.gpy-10.1f)<1e-4f,"Aufprall: gpy -= 0.5");
        check(se.calls==1&&se.se==259&&se.slot==0,"Aufprall: CallPlayerWeaponSeEx(&px, 259, 0)");
        // Das Original ruft die Stimme VOR dem Setzen auf gpy: die Lage liegt noch darunter.
        check(se.at.y<10.6f&&se.at.y>10.6f+fallen-1e-4f&&se.at.x==10.0f,"Aufprall: Stimme an der ungekuerzten Lage");
        // yn war beim Fall auf -1.0 begrenzt.
        check(last_yn>=-1.0f,"Fall: yn bleibt >= -1.0");
        // Fall 2: Drehung um X in Schritten von 1024 bis 16384, Ende bei gpy 10.1.
        check(s.ax==0,"vor Fall 2 keine Drehung");
        passes=0;int turns=0;
        while(s.flg&&passes<400){const int before=s.ax;step(s);++passes;if(s.ax!=before){check(s.ax-before==1024,"Fall 2: ax += 1024");++turns;}}
        check(turns>0&&s.ax<=16384,"Fall 2: ax waechst bis hoechstens 16384");
        check(!s.flg,"Fall 2: der Satz endet");
        check(std::fabs(s.py-10.1f)<1e-4f&&s.yn==0.0f,"Fall 2: py = gpy, yn 0");
        check(take_effect_output().gaps.empty(),"Trockener Fall ohne Luecke");
    }
    {
        // 2. Wasser: st_flg 0x40 und bhCheckWater bremsen und setzen Spritzer 15 type 1.
        water=true;water_top=12.0f;
        ctx={0x40,1,0,0,0,0,8};set_effect_context(ctx);
        EffectSprite s=magazine(0,{10,18,10});
        step(s);
        check(s.stflg&0x100000u,"Wasser: stflg 0x100000");
        check(std::fabs(s.yn-(-0.3f*0.25f-0.01f))<1e-6f,"Wasser: yn * 0.25, danach yn -= 0.01");
        const auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==15&&out.spawns[0].type==1&&out.spawns[0].mdlver==0,"Wasser: Spritzer 15 type 1");
        check(std::fabs(out.spawns[0].scale.x-0.8f)<1e-6f&&std::fabs(out.spawns[0].scale.y-1.5f)<1e-6f&&std::fabs(out.spawns[0].scale.z-0.8f)<1e-6f,"Wasser: Spritzergroesse 0.8/1.5/0.8");
        check(std::fabs(out.spawns[0].position.y-(1.0f+18.0f))<1e-4f,"Wasser: Spritzer bei py + 1 (vor dem Fallschritt)");
        // Im Wasser bremst die Schwerkraft auf 0.01 je Pass und haelt bei -0.3.
        int passes=0;while(s.mode0==1&&s.flg&&passes<600){step(s);++passes;}
        check(s.mode0==2&&s.yn==0.0f,"Wasser: Aufprall ohne Rueckprall (yn 0)");
        check(se.calls==1,"Wasser: kein CallPlayerWeaponSeEx beim Aufsetzen");
        take_effect_output();
        water=false;
    }
    {
        // 3. Nur mdlver != 0 schreibt einmal auf das rechte Waffenmodell.
        ctx={0,1,0,0,0,0,8};set_effect_context(ctx);int hidden=0;
        EffectWorldHooks h;h.hide_weapon_part2=[&]{++hidden;};set_effect_world_hooks(std::move(h));
        for(unsigned version:{0u,1u,2u}){EffectSprite s=magazine(version,{10,18,10});const int before=hidden;
            step(s);step(s);check(hidden==before+(version!=0),"mdlver-Tor oder einmaliger Magazin-Schreibauftrag falsch");
            check(take_effect_output().gaps.empty(),"Gebundener Waffenbesitzer meldet Luecke");}
        set_effect_world_hooks({});EffectSprite s=magazine(1,{10,18,10});step(s);const auto out=take_effect_output();
        bool named=false;for(const auto& g:out.gaps)if(g.find("sys->obwp")!=std::string::npos)named=true;
        check(named,"Fehlender Waffenbesitzer darf nicht unbemerkt bleiben");
    }
    set_effect_world_hooks({});set_effect_water({});set_effect_ground(nullptr,nullptr,nullptr);
    check(argc==2,"data directory required");weapon_owner(argv[1],12);weapon_owner(argv[1],13);
    std::cout<<"bhEff007 magazine PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
