#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "core_random.h"
#include "chunk_blend.h"
#include "weapon_tables.h"
#include "combat.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<.0001f;}
static bool same(NJS_POINT3 a,NJS_POINT3 b){return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z);}
static void flame_states(){
    auto made=lighter_sprite();auto s=made.sprite;EffectContext c;c.player_flg=0x1000000;bool queued=false;
    check(s.id==9&&s.type==2&&s.flg==1&&same(made.scale,{.5f,.5f,.5f}),"WpnEffTab[1][0] differs");
    for(int tick=0;tick<25;++tick){check(step_effect_sprite(s,c,&queued)&&queued,"native lighter did not queue");
        const int key=tick%8;check(s.mode0==1&&s.ct0==key+1&&s.tex_id==8&&!s.ani_ct&&s.flg==0x4100001,"flame flags/texture/cursor differ");
        for(unsigned i=0;i<4;++i)check(s.tv[i].u==(key+((i&1)?1:0))*.0625f&&s.tv[i].v==.8125f+((i&2)?.0625f:0)&&s.tv[i].col==0xffe0e0e0,"compact flame UV/color differs");}
    for(unsigned gate=0;gate<3;++gate){const auto clock=s.ct0;auto paused=c;if(gate==0)paused.pt_flg=0;if(gate==1)paused.player_stflg=0x1000000;if(gate==2)paused.player_mdflg=1;
        check(step_effect_sprite(s,paused,&queued)&&!queued&&s.ct0==clock&&s.flg!=0,"hidden flame advances or frees its slot");}
    c.player_flg=0;check(step_effect_sprite(s,c,&queued)&&!queued&&!s.flg,"unequip does not release flame");
    for(unsigned type:{0u,1u,3u,7u}){EffectSpawn spawn;spawn.id=9;spawn.type=type;initialize_effect_sprite(s,spawn);c.st_flg=0x20000000;
        for(int tick=0;tick<17;++tick){check(step_effect_sprite(s,c,&queued)&&queued,"other original flame type not queued");const int key=tick%8;
            check((s.flg&0x100000u)&&!(s.flg&0x1000000u),"flame billboard/hide flags differ");
            check(s.tex_id==int(type==1||type==3?8:9),"flame UV-bank selection differs");
            check(s.tv[0].u==(type==1||type==3?key*.0625f:(key%4)*.25f)&&s.tv[0].v==(type==1||type==3?.8125f:(key/4)*.25f),"large/small flame UV progression differs");}
        if(type==3){c.st_flg=0;check(step_effect_sprite(s,c,&queued)&&!queued&&!s.flg,"object flame outlives original system flag");}}
    // The original UV pointer is latched, not reselected on type mutation.
    s=made.sprite;c.player_flg=0x1000000;step_effect_sprite(s,c,&queued);s.type=0;step_effect_sprite(s,c,&queued);
    check(s.tex_id==8&&s.tv[0].v==.8125f,"type mutation replaces latched UV bank");
}
static void light_states(){
    const auto light=lighter_light();check(light.flg==1&&light.type==4&&light.aspd==30&&light.lkflg==1&&light.lkono==9&&light.lsrc==4&&same(light.offset,{.3f,0,-1.5f})&&same(light.direction,{0,0,-1})&&same(light.color,{3.16f,2.06f,.7f})&&light.near_range==8&&light.far_range==400,"lgttab[1]/direction/ranges differ");
    for(unsigned type=1;type<=6;++type){std::uint32_t rng=1;unsigned clock=0;NJS_POINT3 working=light.color;recvx_core_random_seed(1);
        for(unsigned tick=0;tick<40;++tick){const float sin=njSin(clock);const unsigned shape=(type-1)%3;const float a=shape==0?.25f:shape==1?.5f:1.f,b=shape==0?.75f:shape==1?.5f:0;
            const NJS_POINT3 expect{sin*a*light.color.x+light.color.x*b,sin*a*light.color.y+light.color.y*b,sin*a*light.color.z+light.color.z*b};
            unsigned delta=7680;if(type>=4){rng=rng*0x41c64e6du+0x3039u;delta=unsigned(7680.f*(-int(rng&0x7fffffff)/-2.1474836e9f));}
            const auto after=(clock+delta)&0x7fff;step_light_color(type,30,clock,light.color,working,true);
            check(clock==after&&same(working,expect),"light sample-before-advance or shared RNG differs");
            const auto paused=working;step_light_color(type,30,clock,{0,0,0},working,false);check(clock==after&&same(working,paused),"paused light advances or recomputes RGB");}
        rng=rng*0x41c64e6du+0x3039u;check(recvx_core_random_next()==int(rng&0x7fffffff),"light consumes wrong number of shared random values");}
}
static void draw_contract(){
    auto s=lighter_sprite().sprite;EffectContext c;c.player_flg=0x1000000;bool queued;step_effect_sprite(s,c,&queued);
    s.flg=(s.flg|0x80u)&~0x100000u;NJS_MATRIX parent;njUnitMatrix(&parent);njRotateY(&parent,16384);njRotateZ(&parent,8192);
    Scene scene;append_effect_sprite(scene,s,{5,6,7},{.5f,.5f,.5f},{},0,parent);NJS_POINT3 p{-.5f,-1,0};njCalcVector(&parent,&p,&p);p.x+=5;p.y+=6;p.z+=7;
    check(scene.triangles.size()==2&&same(scene.triangles[0].vertices[0].position,p),"non-billboard linked effect ignores parent basis");
    Scene filter;filter.background=0xff303030;filter.textures.push_back({2,2,{0xff000000,0xffffffff,0xffffffff,0xff000000}});
    SceneTriangle tri;tri.textured=true;tri.world_lit=true;tri.color=0xffffffff;tri.blend=SceneBlend::Ninja;tri.vertices={SceneVertex{{-4,-4,2},.5f,.5f},SceneVertex{{4,-4,2},.5f,.5f},SceneVertex{{0,4,2},.5f,.5f}};filter.triangles.push_back(tri);
    NJS_MATRIX view;njUnitMatrix(&view);SoftwareRenderer image(32,32);image.set_supersampling(1);
    for(bool default_filter:{false,true}){image.set_texture_filtering(default_filter);filter.triangles[0].texture_filter=SceneTextureFilter::Point;image.draw_view(filter,view,16,16);check(image.pixels()[16*32+16]==0xff000000,"explicit point filter ignored");
        filter.triangles[0].texture_filter=SceneTextureFilter::Linear;image.draw_view(filter,view,16,16);const auto pixel=image.pixels()[16*32+16];
        check((pixel>>24)==255&&std::abs(int(pixel&255)-128)<=1&&((pixel>>8)&255)==(pixel&255)&&((pixel>>16)&255)==(pixel&255),"explicit base-level linear filter/GS alpha ignored");}
}
static void resources(const std::filesystem::path& data){
    const auto system=read_asset_file(data/"archives/SYSTEM/0001.bin",8*1024*1024);const auto offset=common_effect_textures_offset(system);
    check(offset!=std::size_t(-1)&&!(offset&31),"common PVP offset not determined/aligned");
    const auto atlas=decode_texture_bundle(system,offset);const auto categories=common_effect_categories();
    check(categories.size()==20&&categories[8]==std::pair<unsigned,unsigned>{25,1}&&categories[9]==std::pair<unsigned,unsigned>{26,2}&&categories[19]==std::pair<unsigned,unsigned>{39,1}&&atlas.size()==40,"ef_info original category/texture counts differ");
    check(common_effect_textures_offset({})==std::size_t(-1),"absent asset returns usable offset");
    for(const auto bytes:{std::vector<std::uint8_t>{0xff,0xff,0xff,0xff},std::vector<std::uint8_t>{0xff,0xff,0xff,0x7f}}){bool rejected=false;try{common_effect_textures_offset(bytes);}catch(const std::runtime_error&){rejected=true;}check(rejected,"truncated/early-ended model chain accepted");}
    const auto room=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0050");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    world.set_player_weapon(1);world.tick();unsigned slot=0;while(world.effect_state(slot)&&!(world.effect_state(slot)->id==9&&world.effect_state(slot)->type==2))++slot;
    check(world.effect_state(slot)&&world.effect_visible(slot)&&(events.flags[13][0]&0x1000000)&&events.player_flags==events.flags[13][0],"lighter not attached to native player flags/owner");
    const auto lamp=world.light_state(1);check(lamp&&lamp->flags==3&&lamp->type==4&&lamp->link==1&&lamp->index==0&&lamp->node==9&&same(lamp->color,{3.16f*.75f,2.06f*.75f,.7f*.75f}),"lighter light not sampled/linked on production owner");
    for(unsigned turn:{0u,16384u}){actor.yaw=int(turn);std::vector<BoneMatrix> model_nodes;Scene draw=world.sample_player(player,&model_nodes);const auto nodes=player.script_nodes(model_nodes);auto hand=nodes.at(9);
        // bhCalcModel(plp) at the end of bhControlPlayer (player.c:1517) commits
        // this object tree; every link reads plp->mlwP->owP, never the clip.
        world.commit_player_joints(nodes);
        NJS_POINT3 point={.3f,0,-1.5f};njCalcPoint(&hand.value,&point,&point);check(same(world.light_state(1)->position,point),"light offset not transformed by actual hand node");
        const auto count=draw.triangles.size();world.append(draw,model_nodes,stage.cameras.at(0));check(draw.triangles.size()>=count+2,"production flame triangles absent");
        // Die Flamme wird ueber ihren eigenen Atlaseintrag gesucht, nicht
        // ueber die Listenposition: der Effektpass uebernimmt seit
        // effect.c:742 die Spawn-Ausgabe aufsteigend, deshalb koennen im
        // selben Bild erzeugte Arbeiten mit hoeherem Platz nach bhEff009
        // angehaengt werden (RM_0050: drei bhEff182-Kinder der bhEff218-Werke).
        std::size_t at=count;
        for(;at+1<draw.triangles.size();++at)if(draw.triangles[at].texture<draw.textures.size()&&draw.textures[draw.triangles[at].texture].pixels==atlas.at(25).pixels)break;
        check(at+1<draw.triangles.size(),"bhEff009 flame pair absent from the effect pass");
        const auto& t=draw.triangles.at(at);check(t.blend==SceneBlend::Ninja&&t.blend_register==0x44&&!t.depth_write&&t.texture_filter==SceneTextureFilter::Linear,"flame blend/filter/depth contract differs");
        check(draw.textures.at(t.texture).pixels==atlas.at(25).pixels,"production common atlas or alpha was altered");
        const auto* flame=world.effect_state(slot);point=weapon_fire_offset(1);njCalcPoint(&hand.value,&point,&point);
        NJS_MATRIX basis;njUnitMatrix(&basis);const auto camera=stage.cameras.at(0);njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);
        NJS_POINT3 local{flame->tv[0].x*.5f,flame->tv[0].y*.5f,0};njCalcVector(&basis,&local,&local);point.x+=local.x;point.y+=local.y;point.z+=local.z;
        check(same(t.vertices[0].position,point),"flame hand anchor/billboard applies wrong rotation/translation");
        // Real atlas and production geometry rendered in isolation. Runtime
        // acquisition is separately checked by verify_lighter.py.
        Scene only;only.background=0xff101010;only.textures=draw.textures;only.triangles={t,draw.triangles.at(at+1)};NJS_MATRIX view;njUnitMatrix(&view);
        const auto center=t.vertices[2].position;njTranslate(&view,-center.x,-center.y,5-center.z);SoftwareRenderer image(128,128);image.set_supersampling(1);image.draw_view(only,view,200,200);
        check(std::count_if(image.pixels().begin(),image.pixels().end(),[](auto pixel){return pixel!=0xff101010;})>0,"real flame atlas/geometry renders no pixels");
    }
    world.set_simulation_flags(0);const auto before=world.effect_state(slot)->ct0;const auto clock=world.light_state(1)->clock;world.tick();check(world.effect_state(slot)->ct0==before&&world.light_state(1)->clock==clock&&world.effect_visible(slot),"paused effect does not retain its queued frame");
    events.flags[14][0]|=0x1000000;check(!world.effect_visible(slot),"paused hidden-player flame remains drawn");events.flags[14][0]&=~0x1000000u;
    world.set_simulation_flags(~0u);world.set_presentation_flags(0);world.tick();check(!world.effect_visible(slot)&&world.effect_state(slot)->ct0==before,"presentation gate ignored");world.set_presentation_flags(~0u);
    // effect.c:268 bhSetExtraEffectWork nimmt den ersten freien eff[512]-Platz: der eben
    // freigegebene Flammenplatz kann im selben Pass als Hilfsarbeitssatz (flg 2) eines
    // anderen Raumeffekts wiederkehren. Geprueft wird deshalb, dass keine Flamme mehr
    // lebt, nicht dass genau dieser Platz leer bleibt.
    const auto flame_live=[&]{for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&(s->flg&1u)&&s->id==9&&s->type==2)return true;}return false;};
    world.set_player_weapon(0);world.tick();check(!flame_live()&&!world.effect_visible(slot)&&!(events.flags[13][0]&0x1000000)&&!(world.light_state(1)->flags&2),"unequip leaves flame/light allocated");
    // bhSetEffectTb nimmt den ersten freien Platz des geteilten eff[512]-Satzes: welcher das
    // ist, haengt davon ab, was die uebrigen Raumeffekte gerade belegen (auch Hilfsarbeitssaetze).
    world.set_player_weapon(1);world.tick();
    unsigned flame=512;for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&(s->flg&1u)&&s->id==9&&s->type==2){flame=i;break;}}
    check(flame<512&&world.effect_visible(flame)&&world.effect_state(flame)->ct0==1,"reequip does not reset the UV cursor");
    for(unsigned i=0;i<flame;++i){const auto* s=world.effect_state(i);check(s&&(s->flg&3u),"reequip did not take the first free shared slot");}
    events.flags[14][0]|=0x1000000;world.tick();check(!world.effect_visible(flame)&&!(world.light_state(1)->flags&2),"hidden player control leaves its lighter light enabled");events.flags[14][0]&=~0x1000000u;world.tick();check(world.effect_visible(flame)&&(world.light_state(1)->flags&2),"player light does not relink after hidden status ends");
    world.set_player_weapon(0);world.tick();unsigned object=4;while(object<256&&(!world.state(2,object)||!(world.state(2,object)->object_flags&1u)))++object;
    check(object<256,"real room contains no enabled object for object-linked flame test");
    check(world.bind({0xa2,0,0,0,{0xa2,std::uint8_t(object),0,0}}),"object lighter event remains unbound");world.tick();
    // Der eff[512]-Pool ist geteilt: seit die Raumprozeduren dieses Raumes
    // laufen (bhEff181 fordert bhEff182 an), kann der zuvor benutzte Platz
    // schon wieder vergeben sein. Die Objektflamme wird deshalb an ihrer
    // Originalkennung gesucht, nicht an einem festen Index.
    {unsigned found=0;while(world.effect_state(found)&&!(world.effect_state(found)->id==9&&world.effect_state(found)->type==3))++found;
     check(world.effect_state(found),"object lighter event allocated no original flame work");slot=found;}
    check((events.flags[5][0]&0x20000000u)&&world.effect_state(slot)->type==3&&world.effect_visible(slot)&&world.light_state(1)->link==3&&world.light_state(1)->index==object,"object flame/light do not share native object/system owners");
    check(world.bind({0xa2,0,0,0,{0xa2,std::uint8_t(object),1,0}}),"object flame disable unbound");world.tick();
    check(!(events.flags[5][0]&0x20000000u)&&!world.effect_state(slot)->flg&&!world.effect_visible(slot)&&!(world.light_state(1)->flags&2u),"object flame/light do not release on original event flag");
    std::cout<<"Common effect PVP "<<offset<<", 40 textures; native flame/light/hand/alpha/render lifecycle passed.\n";
}
// bhEne01_PlyDG00 case 3 (en01.c:7625-7637) ends the bite: it hands the player
// work back to plp->mnwPb and writes *(int*)&pl->mode0 = 1, but leaves
// plp->mtn_no on the zombie entry (En01_PlyMtn_OffsetTbl[0] + 10/11 = 410/411)
// until bhControlPlayer rewrites it in the same frame (game.c:47).
// bhControlEffect (game.c:48) then reads pp->mlwP->owP[op->lkono].mtx
// (effect.c:803-811) and the player light does the same, so neither may resolve
// plp->mnwP[plp->mtn_no] on that frame.
static void grab_release_links(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_0050.RDT",64*1024*1024);const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0050");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    check(!player.has_motion(410)&&!player.has_motion(411),"the player library owns the zombie grab entries, premise gone");
    world.set_player_weapon(1);
    // bhCalcModel(plp) of the pass before the release (player.c:1517).
    std::vector<BoneMatrix> model_nodes;(void)world.sample_player(player,&model_nodes);
    const auto committed=player.script_nodes(model_nodes);world.commit_player_joints(committed);
    world.tick();
    unsigned slot=0;while(world.effect_state(slot)&&!(world.effect_state(slot)->id==9&&world.effect_state(slot)->type==2))++slot;
    check(world.effect_state(slot)&&world.effect_visible(slot),"lighter flame absent before the release frame");
    for(const std::size_t entry:{std::size_t(410),std::size_t(411)}){
        motion=entry;frame=35*65536;world.commit_player_joints(committed);
        world.tick(); // the released frame: mnwPb active, mtn_no still the zombie entry
        auto owp=committed.at(9); // plp->mlwP->owP[9].mtx
        NJS_POINT3 hand=weapon_fire_offset(1);njCalcPoint(&owp.value,&hand,&hand);
        const auto* flame=world.effect_state(slot);
        check(flame&&close(flame->px,hand.x)&&close(flame->py,hand.y)&&close(flame->pz,hand.z),"released frame does not link the flame to plp->mlwP->owP[9]");
        NJS_POINT3 lamp{.3f,0,-1.5f};njCalcPoint(&owp.value,&lamp,&lamp); // lgttab[1] offset
        check(same(world.light_state(1)->position,lamp),"released frame does not take the player light from the committed owP");
    }
    std::cout<<"PlyDG00 release: effect and light links read plp->mlwP->owP; motion 410/411 never resolved in mnwPb.\n";
}
// light.c:673-683 Typ 100: das Muendungslicht von bhSetGunFire (pwksub.c:2684-2735,
// lgtp[0] mit WpnTab.ltp 100, flg 3, mode 0, an plp->mlwP->owP[jno]) setzt bhControlLight
// im Schusspass (mode 0 -> 1) und im naechsten Pass (flg &= ~0x2, aber noch nach lsrc 4
// light.c:788 gesetzt); ab dem dritten Pass gehoert es nicht mehr zum Lichtsatz.
// Gemessen wird am Raumdreieck, das append() beleuchtet: derselbe Lauf ohne Schuss mit
// demselben Zufallsstrom ist die Vergleichsbasis.
static void gun_light_passes(const std::filesystem::path& data){
    const auto& wt=WpnTab[4];check((wt.flg&0x4u)&&wt.ltp==100&&wt.lnr==5&&wt.lfr==20,"WpnTab[4] light fields differ");
    RECVX_CB_LIGHT gun{};gun.flg=3;gun.aspd=0;gun.lkflg=1;gun.lkno=0;gun.lkono=9;gun.mode=0;gun.lsrc=4;gun.type=wt.ltp;
    gun.r=wt.lr/40.0f;gun.g=wt.lg/40.0f;gun.b=wt.lb/40.0f;gun.nr=wt.lnr;gun.fr=wt.lfr;gun.lx=wt.wp_fps1.x;gun.ly=wt.wp_fps1.y;gun.lz=wt.wp_fps1.z;
    const auto room=read_asset_file(data/"rooms/RM_1000.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto player=PlayerAsset::load(data,10);
    struct Pass {NJS_POINT3 light;unsigned flags;};
    const auto run=[&](bool shoot){
        recvx_core_random_seed(7);auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_1000");world.set_player_asset(&player);
        Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
        std::vector<BoneMatrix> model_nodes;(void)world.sample_player(player,&model_nodes);const auto nodes=player.script_nodes(model_nodes);world.commit_player_joints(nodes);
        world.tick();
        auto hand=nodes.at(9);NJS_POINT3 anchor{gun.lx,gun.ly,gun.lz};njCalcPoint(&hand.value,&anchor,&anchor);
        if(shoot){world.set_gun_light(gun);check(world.light_state(0)&&same(world.light_state(0)->position,anchor),"muzzle light not linked to owP[9] + wp_fps1");}
        std::vector<Pass> passes;
        for(unsigned i=0;i<5;++i){world.tick();
            // Boden 3 unter der Muendung (innerhalb nr 5), Normale zum Licht.
            Scene probe;SceneTriangle t;t.textured=false;t.color=0xffffffffu;
            t.vertices={SceneVertex{{anchor.x-1,anchor.y-3,anchor.z-1}},SceneVertex{{anchor.x+1,anchor.y-3,anchor.z-1}},SceneVertex{{anchor.x,anchor.y-3,anchor.z+1}}};
            for(auto& v:t.vertices)v.normal={0,1,0};probe.triangles.push_back(t);
            world.append(probe,model_nodes,stage.cameras.at(0));passes.push_back({probe.triangles.at(0).vertices[0].light,world.light_state(0)->flags});}
        return passes;};
    const auto base=run(false),shot=run(true);
    const auto brighter=[](NJS_POINT3 a,NJS_POINT3 b){return a.x>b.x+.01f||a.y>b.y+.01f||a.z>b.z+.01f;};
    check(base[0].light.x<1.f||base[0].light.y<1.f||base[0].light.z<1.f,"probe saturated without muzzle light");
    check(shot[0].flags==3&&brighter(shot[0].light,base[0].light),"muzzle light missing in the shot pass (light.c:675 mode 0)");
    check(!(shot[1].flags&2u)&&brighter(shot[1].light,base[1].light),"second pass must clear flg 0x2 and still light (light.c:680/788)");
    for(unsigned i=2;i<5;++i)check(!(shot[i].flags&2u)&&same(shot[i].light,base[i].light),"muzzle light stays in the light set after its two passes");
    std::cout<<"bhSetGunFire type 100: lit in two bhControlLight passes, then off.\n";
}
int main(int argc,char** argv){try{check(argc==2,"original data directory required");_Make_SinTable();flame_states();light_states();draw_contract();resources(argv[1]);grab_release_links(argv[1]);gun_light_passes(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
