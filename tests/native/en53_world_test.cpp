// Echte EN53-Raumbesitzer. Tod/Pausen sind Diagnosevorbedingungen,
// kein natuerlich gespielter Nosferatu-Kampf. Referenz en15.c:3350-3745.
#include "world.h"
#include "room.h"
#include "binary_view.h"
#include "renderer.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene stage;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    Fixture(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),stage(decode_scene(bytes)),actor(stage.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),world(bytes,stage,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};commit();events.initialize();
    }
    void commit(){auto nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    RECVX_ENEMY_WORK* core(){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n;++i)if(t.slot[i]&&t.slot[i]->id==53)return t.slot[i];return nullptr;}
    SceneCamera camera(){auto c=world.camera(stage,stage.cameras[0]);const auto clip=world.camera_clip();c.clip_near=clip.near;c.clip_far=clip.far;return c;}
    Scene draw(){auto scene=stage;std::vector<BoneMatrix> nodes;auto p=world.sample_player(player,&nodes);const auto base=scene.textures.size();scene.textures.insert(scene.textures.end(),player.textures().begin(),player.textures().end());append_actor(scene,p,base);world.append(scene,nodes,camera());return scene;}
    void no_gaps(){for(const auto& s:world.unbound_enemy_routines())throw std::runtime_error(s);}
};
static const PlayerAsset& asset(const WorldEntity& e,unsigned k){const auto* p=k?e.enemy_asset->model(k):e.enemy_asset;check(p!=nullptr,"mbp fehlt");return *p;}
static std::size_t green_byte(const PlayerAsset& a){
    const auto child=a.child(0);check(child>=0,"mbp->child fehlt");BinaryView v(a.encoded_model());const auto base=v.u16(4);
    const auto objects=v.relative(base,v.u32(12),a.bones()*52),mdl=v.relative(base,v.u32(objects+std::size_t(child)*52+4),24);
    const auto plist=v.relative(base,v.u32(mdl+4),12);check(v.u8(plist)==19,"echter Kern: erwarteter DA-Chunk fehlt");return plist+9;
}
static void materials(const WorldEntity& e){
    for(unsigned k=0;k<3;++k){const auto& a=asset(e,k);const auto* live=e.live_stream(k);check(live!=nullptr,"mbp hat keinen beschreibbaren Strom");const auto original=a.live_stream();check(original.size()==live->size(),"Groesse des Live-Stroms");const auto green=green_byte(a);
        for(std::size_t i=0;i<original.size();++i)if(i!=green)check(original[i]==(*live)[i],"ChangeAmbient veraendert ein fremdes Byte");
        check((*live)[green]<=178,"Ambientgruen ausserhalb der Todesrampe");
    }
}
static unsigned rendered_green(const WorldEntity& e){
    const auto& a=asset(e,e.morph_obj_a);auto scene=a.sample_work(e.enemy_obj,e.enemy_owp_mtx,e.enemy_evalflags,nullptr,nullptr,e.live_stream(e.morph_obj_a));
    check(!scene.triangles.empty(),"ausgewaehltes Modell zeichnet nichts");scene.resolve_ambient_material();
    const auto v=scene.triangles[0].ambient_material;check(std::abs(v.x-178.f/255.f)<1e-6f&&std::abs(v.z-178.f/255.f)<1e-6f,"R/B wurden als Fade behandelt");
    check((scene.triangles[0].color>>24)==255,"Ambient als Alpha behandelt");return unsigned(std::lround(v.y*255));
}
int main(int argc,char** argv){try{
    check(argc>=2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    for(const char* room:{"RM_5800","RM_6040","RM_6100"}){
        Fixture f(data,room);auto* epw=f.core();check(epw!=nullptr,"EN53 fehlt");auto& e=*static_cast<WorldEntity*>(epw->owner);
        // initialize steuert vor dem Szenario; das Szenario darf mode0 wieder nullen.
        f.tick();std::cout<<room<<" start "<<int(epw->mode0)<<'/'<<int(epw->mode1)<<" shape="<<epw->shp_ct<<" mdflg="<<epw->mdflg<<" stflg="<<epw->stflg<<'\n';
        check((epw->mdflg&6u)==6u,"CoreInit fehlt");materials(e);
        // RM_6100 setzt nach dem Ladepass stflg 0x1000000 und mode0 0.
        // Nur die Diagnose gibt diesen bereits initialisierten Work wieder frei.
        if(epw->stflg&0x1000000u){epw->stflg&=~0x1000000u;f.tick();}
        check(epw->mode0==1||epw->mode0==5,"CoreInit oder Ereignissteuerung fehlt");
        check(rendered_green(e)==178,"Initiales Gruen");
        // RM_6040 haelt den Kern nach Init in bhEne_Event (mode0 5).
        // Die Pulsabnahme darunter ist dort eine explizite Diagnosevorbedingung.
        if(epw->mode0==5)epw->mode0=1;
        unsigned pairs=0,last=99;
        for(unsigned i=0;i<60;++i){f.tick();const unsigned key=e.morph_obj_a*3+e.morph_obj_b;if(key!=last){last=key;pairs|=1u<<key;}check(rendered_green(e)==178,"Puls liest falschen Strom");}
        check((pairs&(1u<<5))&&(pairs&(1u<<7))&&(pairs&(1u<<3))&&(pairs&(1u<<1)),"Pulsmodellfolge unvollstaendig");f.no_gaps();
        auto scene=f.draw();check(!scene.triangles.empty(),"Raum zeichnet nichts");
        if(argc>2){SoftwareRenderer image(640,480);image.set_supersampling(1);image.draw(scene,f.camera());image.save_bmp(std::filesystem::path(argv[2])/(std::string(room)+"-pulse.bmp"));}
        // Nur EN53 weitersteuern: reproduzierbare Todvorbedingung, keine Kampf-Aussage.
        auto& table=f.world.enemy_table();for(unsigned i=0;i<table.ewk_n;++i)if(table.slot[i]&&table.slot[i]!=epw)table.slot[i]->flg&=~1u;
        epw->mode0=3;epw->mode1=0;epw->mode2=2;epw->mode3=0;epw->shp_ct=0;
        f.tick();check(epw->ct2==174&&rendered_green(e)==178,"Reihenfolge Patch vor Dekrement");
        bool different=false;unsigned midpoint=0;
        for(unsigned i=0;i<48&&epw->mode1!=6;++i){f.tick();materials(e);const auto g=rendered_green(e);if(g<178&&g>0){different=true;midpoint=g;}
            // Der produktive append-Pfad muss denselben Materialwert beleuchten.
            if(i==20){auto mid=f.draw();bool patched=false;
                for(const auto& t:mid.triangles)if(t.material_phase==7&&std::abs(t.ambient_material.y-float(g)/255.f)<1e-6f)patched=true;
                check(patched,"produktiver append liest nicht den gepatchten Kernstrom");
                if(argc>2){SoftwareRenderer image(640,480);image.set_supersampling(1);image.draw(mid,f.camera());image.save_bmp(std::filesystem::path(argv[2])/(std::string(room)+"-fade.bmp"));}}
        }
        check(different&&midpoint>0,"Fade hatte keine Zwischenwerte");check(epw->mode1==6&&(epw->mdflg&1u),"Tod endet nicht verborgen");check(epw->ct2==0,"Fade endet nicht bei 0");
        unsigned debris=0;for(unsigned i=0;i<512;++i){const auto* s=f.world.effect_state(i);if(s&&s->id==250&&(s->flg&3u))++debris;}
        check(debris>0,"kein Mince im echten Effektpool");const auto before=debris;f.tick();debris=0;for(unsigned i=0;i<512;++i){const auto* s=f.world.effect_state(i);if(s&&s->id==250&&(s->flg&3u))++debris;}check(debris<=before,"Tod erzeugt erneut Debris");f.no_gaps();
        std::cout<<room<<": Init/Puls/3 Materialstroeme/Gruen-Fade/Tod PASS\n";
    }
    // Diagnoseempfaenger im produktiven Lichtpass: Material multipliziert nur
    // das Multi-Umgebungslicht, vor clamp(L + A*M). Einzellicht ignoriert M.
    {
        Fixture f(data,"RM_5800");BinaryView v(f.bytes);const auto h=inspect_room(f.bytes).room_offset;
        for(unsigned i=0;i<v.u32(h+0x84);++i){EventCommand c{};c.opcode=0x35;c.bytes={0x35,1,std::uint8_t(i),0};check(f.world.bind(c),"Licht abschalten");}
        for(unsigned k=0;k<4;++k){EventCommand c{};c.opcode=0x44;c.bytes={0x44,10,10,10,std::uint8_t(k)};check(f.world.bind(c),"Ambient setzen");}
        const auto probe=[&](){Scene s;SceneTriangle t;t.object=65000;t.camera_mask=false;t.color=0xffffffffu;t.chunk_ambient=true;
            for(auto& p:t.vertices){p.position={0,0,10};p.normal={0,-1,0};}s.triangles.push_back(t);
            s.ambient_writes.push_back({0,{127.f/255.f,63.f/255.f,31.f/255.f},0,65000,false});
            f.world.append(s,{},f.camera());return s.triangles[0];};
        f.events.flags[11][0]&=~0x80u;const auto multi=probe();
        for(const auto& p:multi.vertices)check(std::abs(p.light.x-127.f/255.f)<1e-6f&&std::abs(p.light.y-63.f/255.f)<1e-6f&&std::abs(p.light.z-31.f/255.f)<1e-6f,"produktiver Multi-Lichtpass ignoriert Material oder Kanal");
        f.events.flags[11][0]|=0x80u;const auto easy=probe();
        // Die Raumkamera dreht (0,-1,0) vom Easy-Richtungslicht weg: nur A=0.2.
        for(const auto& p:easy.vertices)check(std::abs(p.light.x-.2f)<1e-6f&&std::abs(p.light.y-.2f)<1e-6f&&std::abs(p.light.z-.2f)<1e-6f,"Einzellicht faelschlich mit Ambient-Material multipliziert");
        std::cout<<"Lichtpfad: Multi RGB=127/63/31 geteilt durch 255, Einzellicht RGB=0.2 PASS\n";
    }
    // Erneuter Raumaufbau nach Tod: immutable Asset und neuer Work muessen frisch sein.
    {Fixture f(data,"RM_5800");f.tick();auto& e=*static_cast<WorldEntity*>(f.core()->owner);for(unsigned k=0;k<3;++k)check((*e.live_stream(k))[green_byte(asset(e,k))]==178,"Raumwiedereintritt erbt Fade");}
    std::cout<<"EN53 world PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
