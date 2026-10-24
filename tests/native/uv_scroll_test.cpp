// Originaldaten/World- und Blattdienstpruefungen fuer objitm.c:2050, effsub1.c:8011,
// event.c:1965. Diagnoseflags/Einzelereignisstart sind jeweils explizit markiert.
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "binary_view.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace recvx;
static void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
static void put16(std::vector<std::uint8_t>& b,std::size_t p,unsigned v){b[p]=v;b[p+1]=v>>8;}
static void put32(std::vector<std::uint8_t>& b,std::size_t p,unsigned v){put16(b,p,v);put16(b,p+2,v>>16);}
// Unabhaengiger Datenleser: regulaere plist-Kette, nur die in den Zielassets vorhandenen
// 65/66-Streifen ohne Userpayload. Liefert Byteorte (U,V) fuer genaue Short-Gegenprobe.
static std::vector<std::size_t> uv_sites(const std::vector<std::uint8_t>& b,std::size_t model,unsigned object){
    BinaryView v(b);const auto base=model+v.u16(model+4),nodes=base+v.u32(model+12);
    const auto mdl=base+v.u32(nodes+object*52+4);std::size_t p=base+v.u32(mdl+4);std::vector<std::size_t> out;
    for(unsigned guard=0;guard<4096;++guard){const unsigned h=v.u8(p);
        if(h==255)return out;
        if(h<8){p+=2;continue;}if(h<16){p+=4;continue;}
        const auto end=p+2u*(v.u16(p+2)+2u);
        if(h==65||h==66){auto q=p+6;const auto info=v.u16(p+4);check(!(info&0xc000u),"target data with unexpected strip user words");
            for(unsigned n=0;n<(info&0x3fff);++n){const int signed_count=std::int16_t(v.u16(q));q+=2;const unsigned count=std::abs(signed_count);
                for(unsigned i=0;i<count;++i,q+=6){v.require(q,6);out.push_back(q+2);}}
            check(q==end,"target strip size mismatch");}
        p=end;
    }throw std::runtime_error("test unterminated plist");
}
static void expect_offsets(const std::vector<std::uint8_t>& original,const std::vector<std::uint8_t>& changed,const std::vector<std::size_t>& sites,int u,int v){
    auto wanted=original;BinaryView old(original);for(const auto at:sites){put16(wanted,at,unsigned(int(old.u16(at))+u));put16(wanted,at+2,unsigned(int(old.u16(at+2))+v));}
    check(wanted==changed,"UV mutation differs from exact original short offsets or changed non-UV bytes");
}
#ifdef UV_PORT_AFTER
static void primitive(){
    // MDL mit einem Objekt und zwei Texturstreifen (65 und 66), negativem Stripcount,
    // Material-, Textur-, untexturiertem und Volumenchunk dazwischen. Kein Originalcode.
    std::vector<std::uint8_t> b(256,0);b[0]='M';b[1]='D';b[2]='L';b[3]=0x80;put16(b,4,16);put16(b,6,1);put32(b,12,0);
    put32(b,20,52);put32(b,68,0xffffffff);put32(b,72,76); // MDL base16: model68, plist92
    std::size_t p=92;auto word=[&](unsigned a){put16(b,p,a);p+=2;};
    word(1);word(8);word(3);word(17);word(2);word(0xffff);word(0xffff);
    const auto strip=[&](unsigned head,int n){word(head);word(11);word(1);word(unsigned(n));for(int i=0;i<3;++i){word(i);word(32760+i);word(0x8000+i);}};
    strip(65,-3);word(56);word(1);word(0);strip(66,3);word(255);b.resize(p);
    const auto sites=uv_sites(b,0,0);check(sites.size()==6,"synthetic UV count");auto old=b;
    check(offset_model_object_uv(b,0,0,32,-33),"npSetOffsetUV fails realized model");expect_offsets(old,b,sites,32,-33);
    check(offset_model_object_uv(b,0,0,-32,33),"npSetOffsetUV inverse failed");check(b==old,"short wrap inverse does not restore exact model");
    put32(b,20,0xffffffff);old=b;check(!offset_model_object_uv(b,0,0,1,1)&&b==old,"missing model UV update mutates or succeeds");
    std::cout<<"primitive: 65/66, negative strip count, multi-chunk, signed short overflow, inverse PASS\n";
}
static void effect_unit(){
    check(supported_effect_sprite(120),"bhEff120 still missing from production dispatch");
    int angle=0;std::vector<std::pair<unsigned,int>> writes;EffectWorldHooks hooks;
    int marker=0;hooks.work_node_angle_z=[&](void* w,unsigned n,int& a){check(w==&marker&&n==11,"wrong linked node reader");a=angle;return true;};
    hooks.offset_work_uv=[&](void* w,unsigned n,int u,int v){check(w==&marker&&u==0,"wrong linked UV work/U");writes.emplace_back(n,v);return true;};set_effect_world_hooks(hooks);
    EffectSpawn spawn;spawn.id=120;spawn.linked_work=&marker;EffectSprite s;initialize_effect_sprite(s,spawn);EffectContext c;c.gm_flg=1;bool queued=true;
    step_effect_sprite(s,c,&queued);check(s.mode0==0&&writes.empty()&&!queued&&(s.flg&0x1000000u),"mode0 gm_flg1 gate or queue differs");
    c.gm_flg=0;step_effect_sprite(s,c,&queued);check(s.mode0==1&&s.ct0==0&&s.ct1==1024&&s.ct3==0&&s.azp==0,"init timers/angle differs");
    check(writes==std::vector<std::pair<unsigned,int>>{{4,1024},{2,1024}},"init must shift node4 THEN node2 +1024");
    int expected_ct1=1024,previous=0;
    for(int a:{-1,-32,-33,0,31,32,32767,32768,65535,0,-65536,32000}){
        writes.clear();angle=a;const int d=std::int16_t(std::uint16_t(a-previous));const int scroll=d<0?(-d+31)/32:-(d/32);
        expected_ct1+=scroll;std::vector<std::pair<unsigned,int>> expected{{4,scroll},{2,scroll}};
        if(expected_ct1<=0){expected_ct1+=1024;expected.emplace_back(4,1024);expected.emplace_back(2,1024);}
        step_effect_sprite(s,c,&queued);check(s.azp==a&&s.ct0==scroll&&s.ct1==expected_ct1&&!queued,"SRA5/short delta/ct1 wrap differs");check(writes==expected,"scroll write order/wrap correction differs");previous=a;
    }
    writes.clear();c.gm_flg=1;step_effect_sprite(s,c,&queued);check(s.mode0==0&&s.ct1==expected_ct1&&writes==std::vector<std::pair<unsigned,int>>{{4,-expected_ct1},{2,-expected_ct1}},"gm1 exit must undo ct1, not free or reset azp");
    writes.clear();step_effect_sprite(s,c,&queued);check(writes.empty(),"waiting mode reverts twice");c.gm_flg=0;step_effect_sprite(s,c,&queued);check(s.ct1==1024&&s.azp==previous,"reentry must preserve azp and reapply1024");
    set_effect_world_hooks({});take_effect_output();EffectSprite missing;missing.id=120;missing.flg=1;step_effect_sprite(missing,c,&queued);const auto gaps=take_effect_output();check(!gaps.gaps.empty(),"missing model owner silently accepted");
    std::cout<<"effect unit: init/gm1/end/reentry, SRA5 -1/-33, short rollover, ordering, missing-owner PASS\n";
}
#endif
struct Fixture{
    std::filesystem::path data;std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;
    AudioMixer audio;Inventory inv;MessageDialog dialog;PlayerAsset player;RoomWorld world;EventVm events;std::uint32_t head=0;unsigned sensya_calls=0;
    Fixture(const std::filesystem::path& root,const char* name):data(root),bytes(read_asset_file(data/(std::string("rooms/")+name+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),world(bytes,scene,actor,motion,frame,audio,data,{},name),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){if(c.opcode==0x96)++sensya_calls;return world.bind(c);};events.task_started=[this](unsigned l){world.start_task(l);};
        events.animation_frame=[this](unsigned k,unsigned i){return world.frame(k,i);};events.object_position=[this](unsigned k,unsigned i){return world.position(k,i);};
        events.event_camera=[this](unsigned& i,unsigned& k){i=world.event_camera_index;k=world.event_camera_key();return world.event_camera;};
        events.enemy_dead=[this](unsigned i){return world.enemy_dead(i);};events.enemy_health=[this](unsigned i)->std::optional<int>{const auto*e=world.state(1,i);return e?std::optional<int>(e->hp):std::nullopt;};
        commit();
    }
    void commit(){std::vector<BoneMatrix> n;if(world.player_scripted)(void)world.sample_player(player,&n);else n=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(n));}
    void tick(bool scenario=true){if(scenario){events.control_exm();events.tick();}world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    unsigned tank_slot(){for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&s->id==120&&(s->flg&1u))return i;}return 512;}
};
static std::vector<SceneVertex> object_uvs(Fixture& f){
    const auto* tex=f.world.model_textures(2,7);check(tex&&!tex->empty(),"obj613 missing texture list");const auto key=(*tex)[0].pixels.data();Scene rendered;f.world.append(rendered);
    std::vector<SceneVertex> out;for(const auto& t:rendered.triangles)if(t.textured&&t.texture<rendered.textures.size()&&static_cast<const Scene&>(rendered).textures[t.texture].pixels.data()==key)for(const auto& v:t.vertices)out.push_back(v);
    check(!out.empty(),"obj613 missing from native World draw");return out;
}
static void objects(const std::filesystem::path& data,const char* name){
    Fixture f(data,name);const auto* e=f.world.state(2,7);check(e&&e->identity==613&&(e->type&255)==10,"original type10/id613 binding changed");
    // Keine Raumskript-Ersetzung: nur Flags als gekennzeichnete Blattprobe.
    f.world.set_enemy_ai(false);f.events.flags[6][0]=4;f.world.set_simulation_flags(4);f.events.flags[4][0]=0;
    f.tick(false);const auto base=object_uvs(f);check(e->object_ct0==0,"bhObj010 ran without rm_flg31");
    f.events.set_flag(4,31,0);f.tick(false);check(e->object_ct0==32,"bhObj010 not dispatched by real object type10/rm_flg31");
    auto after=object_uvs(f);check(after.size()==base.size(),"UV scroll changes geometry size");
    for(unsigned i=0;i<base.size();++i){check(std::fabs((after[i].u-base[i].u)-32.f/1023.f)<1e-6f&&after[i].v==base[i].v,"object613 rendered U does not reflect +32 short UV");check(std::memcmp(&after[i].position,&base[i].position,sizeof(NJS_POINT3))==0,"UV scroll changes object matrix/position");}
    const auto first=after;f.events.set_flag(4,31,1);for(unsigned i=0;i<7;++i)f.tick(false);check(e->object_ct0==32,"rm_flg gate advances timer");
    after=object_uvs(f);for(unsigned i=0;i<base.size();++i)check(after[i].u==first[i].u,"rm_flg gate changes UV");
    f.events.set_flag(4,31,0);f.events.flags[10][0]|=0x40000000u;f.tick(false);check(e->object_ct0==32,"cb_flg gate must limit to first4 objects");f.events.flags[10][0]&=~0x40000000u;
    f.events.flags[6][0]=0;f.tick(false);check(e->object_ct0==32,"sp_flg gate advances scroll");f.events.flags[6][0]=4;
    check(f.world.bind({0x24,0,0,0,{0x24,2,7,0}}),"object hide event missing");f.tick(false);check(e->object_ct0==32,"hidden object advances scroll");
    check(f.world.bind({0x24,0,0,0,{0x24,2,7,1}}),"object show event missing");
    for(unsigned i=1;i<32;++i)f.tick(false);check(e->object_ct0==0,"32 passes must wrap ct0");after=object_uvs(f);for(unsigned i=0;i<base.size();++i)check(after[i].u==base[i].u&&after[i].v==base[i].v,"32 passes do not restore exact base UV");
    f.tick(false);after=object_uvs(f);check(e->object_ct0==32,"repeat cycle does not restart");
    // Raumfreigabe/Neuladen: neuer Besitzer aus denselben unveraenderten Bytes;
    // keine UVs oder Timer des alten RoomWorld duerfen ins Asset zurueckgeschrieben sein.
    Fixture reloaded(data,name);reloaded.world.set_enemy_ai(false);reloaded.events.flags[6][0]=4;
    reloaded.tick(false);const auto fresh=object_uvs(reloaded);check(fresh.size()==base.size(),"reload object draw count");
    for(unsigned i=0;i<base.size();++i)check(fresh[i].u==base[i].u&&fresh[i].v==base[i].v,"reload retained old room UV scroll");
    check(reloaded.world.state(2,7)->object_ct0==0,"reload retained object timer");
    std::cout<<name<<" object613: U "<<base[0].u<<" -> "<<first[0].u<<" -> cycle32 "<<base[0].u<<", V "<<base[0].v<<", ct0 0/32/0, flags PASS\n";
}
static void tank(const std::filesystem::path& data,const char* name,bool natural){
    Fixture f(data,name);const bool rm3020=std::string(name)=="RM_3020";
    // RM3020: echter scd1-Eintritt mit mitgebrachtem ev_flg265. Keine eff-/Motion-Injektion.
    // RM7000: Originalevent1 gezielt per Start (Diagnose), da die vollstaendige
    // Untersuchungskamera/Dialogkette hier nicht Teil dieses Tests ist.
    if(rm3020)f.events.set_flag(1,265,0);f.events.initialize();
    if(!natural){f.events.start(0,1);f.events.flags[6][0]|=0x10u;}
    const auto* e=f.world.state(1,0);check(e&&e->identity==65&&e->enemy_asset&&e->subplayer_bound,"tank work not naturally loaded as bhSubpl65");
    const auto original=e->enemy_asset->live_stream();std::vector<std::size_t> sites;
    for(unsigned node:{4u,2u}){const auto one=uv_sites(original,0,node);check(!one.empty(),"track model missing UVs");sites.insert(sites.end(),one.begin(),one.end());}
    const auto* textures=f.world.model_textures(1,0);check(textures&&textures->size()>9,"tank textures missing");for(unsigned i=5;i<=9;++i)check(!(*textures)[i].clamp_u&&!(*textures)[i].clamp_v,"original tank image does not have REPEAT header");
    bool moved=false,created=false;unsigned slot=512;int moved_count=0;float v0=0,v1=0;int observed_ct=0;
    for(unsigned pass=0;pass<(rm3020?550u:320u);++pass){if(!natural)f.events.flags[6][0]|=0x10u;f.tick();slot=f.tank_slot();if(slot==512)continue;created=true;const auto& s=*f.world.effect_state(slot);
        check(s.mode0==1&&(s.flg&0x1000000u)&&!s.frame_queued&&!s.custom_queued,"tank controller must not be drawn");
        check(!e->live_model.empty(),"bhEff120 did not reach authoritative native model stream");expect_offsets(original,e->live_model,sites,0,s.ct1);
        if(s.ct0){moved=true;++moved_count;if(!observed_ct){BinaryView a(original),b(e->live_model);v0=float(std::int16_t(a.u16(sites[0]+2)))/1023;v1=float(std::int16_t(b.u16(sites[0]+2)))/1023;observed_ct=s.ct1;}}
    }
    check(created&&f.sensya_calls==1,"original EventVm opcode96 did not allocate exactly one track controller");check(moved&&moved_count>20,"native wheel animation does not drive track UV");
    for(const auto& c:f.events.pending_bindings)check(c.opcode!=0x96,"bhEffectSensyaSet still reported unbound");
    for(const auto& g:f.world.unbound_enemy_routines())check(g.find("bhEff120")==std::string::npos&&g.find("npSetOffsetUV")==std::string::npos,"track owner gap reached");
    std::cout<<name<<" "<<(natural?"natural scd1/event1":"diagnostic original event1")<<": slot "<<slot<<", changed passes "<<moved_count<<", first moving raw V "<<v0<<" -> "<<v1<<", ct1 "<<observed_ct<<"\n";
#ifdef UV_PORT_AFTER
    // Renderer bind: CPU/OT routing is explicit at bhPutModel, not inferred at decode.
    Scene rendered;f.world.append(rendered);unsigned opaque=0,alpha=0;
    const auto key=(*textures)[9].pixels.data();for(const auto& t:rendered.triangles)if(t.textured&&t.texture<rendered.textures.size()&&static_cast<const Scene&>(rendered).textures[t.texture].pixels.data()==key){++alpha;check(t.texture_cache_clamp,"tank UA does not use OT CLAMP");}
    const auto key2=(*textures)[5].pixels.data();for(const auto& t:rendered.triangles)if(t.textured&&t.texture<rendered.textures.size()&&static_cast<const Scene&>(rendered).textures[t.texture].pixels.data()==key2){++opaque;check(!t.texture_cache_clamp,"tank opaque track unexpectedly OT clamped");}
    check(alpha==204&&opaque>0,"tank real draw route triangle counts changed");
    const auto decoded=decode_model_asset(e->live_model,0,0);
    for(unsigned texture=5;texture<=9;++texture){std::vector<std::pair<float,float>> expected,actual;
        for(const auto& t:decoded.triangles)if(t.textured&&t.texture==texture)for(const auto& v:t.vertices)expected.emplace_back(v.u,v.v);
        const auto pixels=(*textures)[texture].pixels.data();
        for(const auto& t:rendered.triangles)if(t.textured&&t.texture<rendered.textures.size()&&static_cast<const Scene&>(rendered).textures[t.texture].pixels.data()==pixels)
            for(const auto& v:t.vertices)actual.emplace_back(v.u,v.v);
        std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());check(expected==actual,"bhPutModel draws immutable UVs instead of native live_model");}

    std::cout<<"  bhPutModel route: texture9 "<<alpha<<" OT CLAMP triangles, texture5 "<<opaque<<" REPEAT triangles\n";
#endif
    // gm_flg1 restores accumulated offset, retains work/angle, then re-entry adds1024.
    const auto azp=f.world.effect_state(slot)->azp;f.events.flags[11][0]|=1u;f.events.flags[6][0]=8;f.tick(false);check(f.world.effect_state(slot)->mode0==0,"gm1 exit mode");check(e->live_model==original,"gm1 exit fails to restore ALL original UV bytes");
    f.events.flags[11][0]&=~1u;f.tick(false);check(f.world.effect_state(slot)->ct1==1024&&f.world.effect_state(slot)->azp==azp,"gm1 reentry resets previous angle");expect_offsets(original,e->live_model,sites,0,1024);
    f.world.clear_event_effects();check(f.world.effect_state(slot)->flg&1u,"bhClearEventEffect incorrectly frees120");
}
#ifdef UV_PORT_AFTER
static void route_unit(){
    std::vector<SceneTriangle> t(5);t[0].cnk_strip_head=0;t[1].cnk_strip_head=8;t[2].cnk_strip_head=16;t[3].cnk_strip_head=24;t[4].cnk_strip_head=-1;
    for(unsigned dt=0;dt<4;++dt){route_model_texture_cache(t,dt);check(!t[0].texture_cache_clamp&&!t[2].texture_cache_clamp&&!t[4].texture_cache_clamp&&t[3].texture_cache_clamp,"UA/DB route mismatch");check(t[1].texture_cache_clamp==!(dt&1),"EasyMulti/SimpleMulti draw mode route mismatch");
        // pCnkCsVu1FuncTbl (ps2_NinjaCnk.c:106): nur Simple (draw_tp 1/3) ohne DB 0x10 waehlt *Single.
        check(t[0].single_sided==bool(dt&1)&&t[1].single_sided==bool(dt&1)&&!t[2].single_sided&&!t[3].single_sided&&!t[4].single_sided,"Single/Double strip route mismatch");}
    {// ps2_Vu1Strip.c:939: Single verwirft ein Dreieck mit 0 < Normal-z im Kameraraum an allen Ecken, Double nicht.
     Scene s;s.background=0xff000000;SceneTriangle q;q.color=0xffffffff;q.world_lit=true;
     q.vertices={SceneVertex{{-1,-1,2},0,0,{0,0,1}},SceneVertex{{1,-1,2},0,0,{0,0,1}},SceneVertex{{0,1,2},0,0,{0,0,1}}};s.triangles.push_back(q);NJS_MATRIX view;njUnitMatrix(&view);
     SoftwareRenderer r(64,64);r.set_supersampling(1);r.set_render_threads(1);
     r.draw_view(s,view,32,32);check(r.pixels()[32*64+32]!=0xff000000u,"double-sided triangle not drawn");
     s.triangles[0].single_sided=true;r.draw_view(s,view,32,32);check(r.pixels()[32*64+32]==0xff000000u,"single-sided back face drawn");
     for(auto& v:s.triangles[0].vertices)v.normal={0,0,-1};r.draw_view(s,view,32,32);check(r.pixels()[32*64+32]!=0xff000000u,"single-sided front face culled");}
    // Renderer test: same immutable texture, repeat header. Only the triangle route
    // changes; nearest and bilinear must clamp consistently at both axes.
    Scene s;s.background=0xff000000;SceneTexture tex;tex.width=tex.height=2;tex.pixels={0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};s.textures.push_back(tex);
    SceneTriangle q;q.textured=true;q.texture=0;q.color=0xffffffff;q.world_lit=true;q.vertices={SceneVertex{{-1,-1,2},1.3f,1.3f},SceneVertex{{1,-1,2},1.3f,1.3f},SceneVertex{{0,1,2},1.3f,1.3f}};s.triangles.push_back(q);NJS_MATRIX view;njUnitMatrix(&view);
    for(bool filter:{false,true}){SoftwareRenderer r(64,64);r.set_supersampling(1);r.set_texture_filtering(filter);r.set_render_threads(1);r.draw_view(s,view,32,32);const auto repeat=r.pixels()[32*64+32];s.triangles[0].texture_cache_clamp=true;r.draw_view(s,view,32,32);check(r.pixels()[32*64+32]==0xffffffff&&repeat!=0xffffffff,"triangle OT clamp not consumed by renderer sampling");s.triangles[0].texture_cache_clamp=false;}
    check(!s.textures[0].clamp_u&&!s.textures[0].clamp_v,"route rewrites texture flags");std::cout<<"route: draw_tp0..3 UA/DB, no default, nearest/bilinear OT CLAMP PASS\n";
}
#endif
int main(int argc,char**argv){try{check(argc==3,"data and case required");_Make_SinTable();std::filesystem::path data=argv[1];std::string test=argv[2];
#ifdef UV_PORT_AFTER
    if(test=="primitive")primitive();else if(test=="effect")effect_unit();else if(test=="route")route_unit();else
#endif
    if(test=="object5600")objects(data,"RM_5600");else if(test=="object6050")objects(data,"RM_6050");else if(test=="tank3020")tank(data,"RM_3020",true);else if(test=="tank7000")tank(data,"RM_7000",false);else if(test=="dispatch")check(supported_effect_sprite(120),"bhEff120 still missing from production dispatch");else throw std::runtime_error("unknown case");
    std::cout<<"uv_scroll "<<test<<" PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"uv_scroll FAIL: "<<e.what()<<"\n";return 1;}}
