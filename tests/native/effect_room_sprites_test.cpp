// Portierte Raum-Effektprozeduren: bhEff122/123/125/126/181/182/218/230 sowie
// die Handfassung effsub0/1/5/6 (native/effect_sprite_procedures.cpp).
// Geprueft werden Zustandsautomat, Originaldaten und die tatsaechliche
// Zeichenausgabe auf dem produktiven RoomWorld-Besitzer mit Originalraeumen.
#include "world.h"
#include "room.h"
#include "effect_sprites_internal.h"
#include "asset_io.h"
#include "binary_view.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include "chunk_blend.h"
#include "cnk_light.h"
#include "weapon_flash.h"
#include "weapon_tables.h"
#include "player_combat_core.h"
#include "renderer.h"
#include "effect_eff130.h"
#include "effect_collision.h"
#include <functional>
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool close(float a,float b){return std::fabs(a-b)<.0001f;}

static EffectSprite work(unsigned id,unsigned type,NJS_POINT3 scale={1,1,1}){
    EffectSpawn spawn;spawn.id=id;spawn.type=type;spawn.scale=scale;
    EffectSprite s;initialize_effect_sprite(s,spawn);return s;
}

// effsub1.c:8158 — weltfeste Platte, Texturgruppe 41, ani_ct = type.
static void sign_plate(){
    EffectContext c;bool queued=false;
    for(unsigned type=0;type<7;++type){
        auto s=work(122,type);
        check(step_effect_sprite(s,c,&queued)&&queued,"bhEff122 did not enter the translucent list");
        check(s.tex_id==41&&s.ani_ct==type&&s.mode0==1&&s.flg==(1u|0x4000000u),"bhEff122 texture/flags differ");
        check(s.bl_src==8&&s.bl_dst==6,"bhEff122 blend pair differs");
        for(const auto& v:s.tv)check(v.col==0xffffffffu,"bhEff122 vertex colour differs");
        // uvinfo_p0: drei Kacheln 0.125 breit, mode1 waehlt sie.
        for(unsigned cell=0;cell<3;++cell){
            s.mode1=cell;step_effect_sprite(s,c,&queued);
            check(close(s.tv[0].u,cell*.125f)&&close(s.tv[0].v,0)&&close(s.tv[3].u,cell*.125f+.125f)&&close(s.tv[3].v,.125f),"bhEff122 uvinfo_p0 cell differs");
        }
        // RM_4020 setzt mode1 = 3. Das Original liest dann die Zeile, die im
        // US-Release direkt hinter uvinfo_p0 liegt (0x00326B90, bhEff123).
        s.mode1=3;step_effect_sprite(s,c,&queued);
        check(close(s.tv[0].u,0)&&close(s.tv[0].v,.875f)&&close(s.tv[3].u,.0625f)&&close(s.tv[3].v,.9375f),"bhEff122 mode1 3 does not read the original following rodata row");
        s.mode1=22;bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"bhEff122 accepts a UV cell outside the documented original window");
    }
}

// effsub1.c:8208 — vier Bilder blinken, danach eine zufaellige Pause.
static void blinking_lamp(){
    EffectContext c;bool queued=false;auto s=work(123,2);
    check(step_effect_sprite(s,c,&queued)&&queued,"bhEff123 first frame not queued");
    check(s.tex_id==2&&s.bl_src==8&&s.bl_dst==10&&s.flg==(1u|0x4100000u),"bhEff123 texture/blend/flags differ");
    for(const auto& v:s.tv)check(v.col==0xff00ff00u,"bhEff123 type-2 colour differs");
    check(s.ct0==1&&s.az==0,"bhEff123 first frame cursor/angle differ");
    // mode0 1 ist die Pause: kein Bild, bis ct1 ueber ct2 laeuft.
    step_effect_sprite(s,c,&queued);check(!queued&&s.mode0==2&&s.ct2>=20&&s.ct2<=30,"bhEff123 pause length outside the original range");
    for(unsigned frame=1;frame<4;++frame){
        step_effect_sprite(s,c,&queued);
        check(queued&&s.az==int(frame*4096)&&close(s.tv[0].u,frame*.0625f)&&close(s.tv[0].v,.875f),"bhEff123 frame/rotation/UV differ");
    }
    check(s.ct0==0&&s.mode0==1,"bhEff123 does not return to its pause after four frames");
    unsigned blank=0;while(blank<64){step_effect_sprite(s,c,&queued);if(queued)break;++blank;}
    check(blank>=20&&blank<=31,"bhEff123 blank interval outside the original 20..30 frames");
    for(const unsigned type:{1u,3u,4u,5u,9u}){
        auto lamp=work(123,type);step_effect_sprite(lamp,c,&queued);
        const std::uint32_t expect=type==1?0xffff0000u:type==3?0xff0000ffu:type==4?0xff00ffffu:type==5?~0xffu:0xffffffffu;
        check(lamp.tv[0].col==expect,"bhEff123 colour table differs");
    }
}

// effsub5.c:1609 — 17 Feuervarianten auf den Texturgruppen 70..79.
static void fire(){
    EffectContext c;bool queued=false;
    auto idle=work(218,0);
    check(step_effect_sprite(idle,c,&queued)&&!queued&&(idle.flg&0x1000000u),"bhEff218 without type is not hidden");
    idle.mode1=4;step_effect_sprite(idle,c,&queued);
    check(queued&&idle.type==4&&!(idle.flg&0x1000000u),"bhEff218 does not take its type from the script mode1");
    recvx_core_random_seed(1);
    auto s=work(218,1);
    check(step_effect_sprite(s,c,&queued)&&queued,"bhEff218 did not enter the translucent list");
    check(s.tex_id==70&&s.ani_ct==0&&s.bl_src==8&&s.bl_dst==3&&s.flg==(1u|0x4180000u),"bhEff218 texture/blend/flags differ");
    check(close(s.tv[0].x,-1)&&close(s.tv[0].y,-2)&&close(s.tv[2].y,0)&&close(s.tv[3].x,1),"bhEff218 quad differs");
    // Fire13: zehn Bilder 40x40 ab (0,0); u-Kante ist w-1, v-Kante +h.
    check(s.ct0>=0&&s.ct0<10,"bhEff218 random start frame outside its table");
    for(unsigned frame=0;frame<24;++frame){
        const int before=s.ct0;step_effect_sprite(s,c,&queued);
        check(queued&&s.ct0==(before+1)%10,"bhEff218 frame does not wrap on its table length");
        const float u=(s.ct0%6)*40.f,v=float(s.ct0/6)*40.f;
        check(close(s.tv[0].u,u/256.f)&&close(s.tv[1].u,(u+39.f)/256.f),"bhEff218 u range differs");
        check(close(s.tv[0].v,v?(v+1.f)/256.f:0.f)&&close(s.tv[2].v,(v+40.f)/256.f),"bhEff218 v range differs");
    }
    // Alle 17 Varianten adressieren die originalen Gruppen 70..79.
    const int texture[17]={70,70,70,71,71,71,72,72,73,73,74,75,76,77,78,78,79};
    for(unsigned type=1;type<=17;++type){
        auto variant=work(218,type);step_effect_sprite(variant,c,&queued);
        check(variant.tex_id==texture[type-1],"bhEff218 variant texture group differs");
    }
    auto wrapped=work(218,18);step_effect_sprite(wrapped,c,&queued);
    check(wrapped.tex_id==70,"bhEff218 does not wrap its variant index at 17");
}

// effsub5.c:3333 — vier UV-Baenke, lkono waehlt sie, (type-1)%6 die Tafel.
static void panel(){
    EffectContext c;bool queued=false;
    for(unsigned bank=0;bank<4;++bank){
        auto s=work(230,3);s.lkono=int(bank);
        check(step_effect_sprite(s,c,&queued)&&queued,"bhEff230 did not enter the translucent list");
        check(s.tex_id==414&&s.ani_ct==2&&s.bl_src==8&&s.bl_dst==6&&s.flg==(1u|0x4080000u),"bhEff230 texture/blend/flags differ");
        const unsigned count=bank==3?3u:8u,row=bank==3?0u:32u*(bank+1);
        for(unsigned frame=0;frame<count*2;++frame){
            const int before=s.ct0;step_effect_sprite(s,c,&queued);
            check(s.ct0==(before+1)%int(count),"bhEff230 frame does not wrap on its bank length");
            check(close(s.tv[0].u,s.ct0*32.f/256.f)&&close(s.tv[1].u,(s.ct0*32.f+31.f)/256.f),"bhEff230 u range differs");
            check(close(s.tv[0].v,row/256.f)&&close(s.tv[2].v,(row+31.f)/256.f),"bhEff230 v range differs");
        }
    }
    auto hidden=work(230,0);step_effect_sprite(hidden,c,&queued);
    check(!queued&&(hidden.flg&0x1000000u),"bhEff230 without type is not hidden");
}

// effsub0.c:1916/1952 — Raucherzeuger und Rauchwolke.
static void smoke(){
    EffectContext c;bool queued=false;
    auto source=work(181,12,{7,7,5.6f});
    take_effect_output();
    step_effect_sprite(source,c,&queued);
    auto out=take_effect_output();
    check(!queued&&(source.flg&0x1000000u),"bhEff181 draws instead of only emitting");
    check(out.spawns.size()==1&&out.spawns[0].id==182&&out.spawns[0].type==11&&out.spawns[0].flg==0x4100001u,"bhEff181 request differs from setentry");
    check(source.ct1==4,"bhEff181 interval does not follow floor(sz)");
    for(unsigned frame=0;frame<4;++frame){step_effect_sprite(source,c,&queued);check(take_effect_output().spawns.empty(),"bhEff181 emits inside its interval");}
    step_effect_sprite(source,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff181 does not emit again after floor(sz) frames");
    auto single=work(181,12,{1,1,0.5f});step_effect_sprite(single,c,&queued);
    check(take_effect_output().spawns.size()==1&&single.type==0&&single.mode1==0,"bhEff181 with floor(sz)==0 does not emit exactly once");
    // Rauchwolke: Farbsatz type%10, Form type/10, Vorschub njFraction(sz).
    auto puff=work(182,11,{2,2,5.6f});puff.ay=16384;
    check(step_effect_sprite(puff,c,&queued)&&queued,"bhEff182 did not enter the translucent list");
    check(puff.tex_id==400&&puff.bl_src==8&&puff.bl_dst==6&&puff.ani_ct==0,"bhEff182 texture/blend differ");
    check(close(puff.px,njFraction(5.6f))&&close(puff.pz,0),"bhEff182 does not advance along its own angles");
    check(close(puff.sx,4.f*2.f*.125f)&&close(puff.sy,4.f*2.f*.125f),"bhEff182 size does not follow its UV tile");
    check(puff.tv[0].col==0xd0201010u&&puff.tv[1].col==0x70302a2au,"bhEff182 kcol row differs");
    unsigned frames=1;while(frames<64&&puff.flg){step_effect_sprite(puff,c,&queued);++frames;}
    check(!puff.flg&&frames==23,"bhEff182 does not release its slot at the original table end");
}

// effsub1.c:8458/8488 — Erzeuger und aufsteigende Wolke mit Bodenabbruch.
static void steam(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_0090.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0090");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    unsigned slot=0;while(world.effect_state(slot)&&world.effect_state(slot)->id!=125)++slot;
    check(world.effect_state(slot),"RM_0090 has no original bhEff125 work");
    world.tick();
    check(!world.effect_visible(slot)&&(world.effect_state(slot)->flg&0x1000000u),"bhEff125 is visible without its script release");
    // Skriptbefehl 0x91 (bhEffModeSet) gibt den Erzeuger frei.
    check(world.bind({0x91,0,0,0,{0x91,std::uint8_t(slot),3,0}}),"bhEffModeSet remains unbound");
    const auto before=world.effect_state(slot)->mode1;check(before==3,"bhEffModeSet did not reach the work");
    world.tick();
    check(world.effect_state(slot)->mode1==0,"bhEff125 does not consume its script release");
    unsigned child=0;bool found=false;
    while(world.effect_state(child)){if(world.effect_state(child)->id==126){found=true;break;}++child;}
    check(found,"bhEff125 did not request the original bhEff126 work");
    // effect.c:742 laeuft aufsteigend und schreibt den Spawn unmittelbar in
    // eff[512]. Der Kindplatz liegt ueber dem Erzeuger, also laeuft seine
    // Prozedur im selben Pass; ein zusaetzlicher Tick waere die Abweichung.
    check(child>slot,"bhEff125 child did not land on a higher shared slot");
    const auto* cloud=world.effect_state(child);
    check(cloud->tex_id==35&&cloud->ani_ct==1&&cloud->mode0==1,"bhEff126 texture/animation selection differs");
    check(cloud->yn<0&&cloud->gpy>-1000.0f,"bhEff126 has no original ground reference");
    const float first=cloud->py;world.tick();
    check(world.effect_state(child)->py<first,"bhEff126 does not rise/sink along its original speed");
}

// RM_0040 Event_init 10 setzt die Schalteranzeigen mit CommonCtr 7 auf
// (1,11.5,4.95), (1,11.5,4.55), (1,11.1,4.95), (1,11.1,4.55).
// Die Werte kommen aus den achtbyte-Auftraegen des RDT, nicht aus Ersatzgrafik.
// event.c:12034..12065 schreibt eff[] selbst; der naechste Control-Pass darf
// den Ort nicht wieder aus der urspruenglichen Raumtabelle herstellen.
static void power_switch_positions(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0040");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    const BinaryView bytes(room);const auto base=inspect_room(room).event_offset;
    const auto script=base+bytes.u32(base+8+10*4);
    check(script==base+2578,"RM_0040 Event_init 10 offset differs");
    // Nur die tatsaechlichen Platzierungsauftraege dieses Originalskripts
    // abspielen; keine synthetischen Lampenerzeuger oder Raumsonderfaelle.
    for(unsigned k=0;k<4;++k){const auto p=script+k*20;
        check(bytes.u8(p)==0x65&&bytes.u8(p+1)==4&&bytes.u8(p+2)==15+k,"power switch work selection differs");
        for(const auto [offset,length]:std::array<std::pair<unsigned,unsigned>,3>{{{0,4},{4,6},{10,8}}}){
            const auto q=p+offset;
            check(world.bind({bytes.u8(q),2,10,q-base,{room.begin()+q,room.begin()+q+length}}),"power switch placement command unbound");
        }
    }
    for(unsigned pass=0;pass<2;++pass){world.tick();
        for(unsigned k=0;k<4;++k){const auto* s=world.effect_state(15+k);const auto* e=world.state(4,15+k);
            check(s&&e&&s->id==230&&s->tex_id==414&&world.effect_visible(15+k),"original switch display missing");
            const float y=k<2?11.5f:11.1f,z=k%2==0?4.95f:4.55f;
            check(close(s->px,1)&&close(s->py,y)&&close(s->pz,z),"CommonCtr effect position lost after ControlEffect");
            check(close(e->transform.position.x,s->px)&&close(e->transform.position.y,s->py)&&close(e->transform.position.z,s->pz),"effect Entity differs from its work");
        }
    }
    Scene draw;world.append(draw,{},stage.cameras.at(0));
    for(unsigned k=0;k<4;++k){bool found=false;
        for(std::size_t i=0;i+1<draw.triangles.size();++i){const auto& a=draw.triangles[i];const auto& b=draw.triangles[i+1];
            if(a.blend!=SceneBlend::Ninja||!a.textured||a.depth_write||a.texture!=b.texture)continue;
            const auto p=a.vertices[0].position,q=b.vertices[1].position;
            if(close((p.x+q.x)/2,1)&&close((p.y+q.y)/2,k<2?11.5f:11.1f)&&close((p.z+q.z)/2,k%2==0?4.95f:4.55f)){
                check(!draw.textures.at(a.texture).pixels.empty(),"switch display has no original texture");found=true;break;}
        }
        check(found,"switch work position did not reach the actual scene");
    }
}

// Produktive Zeichenausgabe mit Originalraeumen und echten Texturen.
static void production(const std::filesystem::path& data){
    struct Case {const char* name;unsigned id;} cases[]={{"RM_7230",122},{"RM_0100",218}};
    for(const auto& item:cases){
        const auto room=read_asset_file(data/("rooms/"+std::string(item.name)+".RDT"),64*1024*1024);
        const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
        std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(room,stage,actor,motion,frame,audio,data,{},item.name);
        Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
        world.tick();
        unsigned slot=0,visible=0;while(const auto* s=world.effect_state(slot)){if(s->id==item.id&&world.effect_visible(slot))++visible;++slot;}
        check(visible>0,"no original room effect of this kind became visible");
        Scene draw;std::vector<BoneMatrix> nodes;
        world.append(draw,nodes,stage.cameras.at(0));
        unsigned quads=0;
        for(const auto& t:draw.triangles)if(t.blend==SceneBlend::Ninja&&t.textured&&!t.depth_write)++quads;
        check(quads>=visible*2,"the visible works produced no original effect triangles");
        // Die Originaltextur muss im Bild ankommen, nicht nur die Geometrie.
        const auto& t=draw.triangles.back();
        check(t.texture<draw.textures.size()&&!draw.textures[t.texture].pixels.empty(),"effect triangle without its original atlas");
        Scene only;only.background=0xff101010;only.textures=draw.textures;
        only.triangles={draw.triangles[draw.triangles.size()-2],t};
        NJS_MATRIX view;njUnitMatrix(&view);const auto center=t.vertices[2].position;
        njTranslate(&view,-center.x,-center.y,8-center.z);
        SoftwareRenderer image(128,128);image.set_supersampling(1);image.draw_view(only,view,200,200);
        check(std::count_if(image.pixels().begin(),image.pixels().end(),[](auto pixel){return pixel!=0xff101010;})>0,
              "real room effect atlas/geometry renders no pixels");
    }
}

// bhEff003/bhSetGunFire: das Muendungsfeuer liegt in der gemeinsamen
// Effekttexturgruppe 3 (ef_info). Geprueft wird, dass die Originalarbeit mit
// der echten Textur des Raumbesitzers tatsaechlich Pixel erzeugt.
static void muzzle_flash(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_0030.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0030");
    const auto* atlas=world.weapon_flash_textures();
    check(atlas&&atlas->size()>=2,"common effect texture group 3 (muzzle flash) missing");
    const auto& effect=WpnEffTab[4][0];
    check(effect.flg&&effect.id==3,"WpnEffTab of the original handgun no longer requests bhEff003");
    WeaponFlash flash;flash.flg=effect.flg;flash.type=effect.type;flash.flr_no=effect.flr_no;
    flash.mdlver=effect.mdlver;flash.sx=effect.sx;flash.sy=effect.sy;flash.sz=effect.sz;
    njUnitMatrix(&flash.matrix);flash.flg|=0x1000;
    flash.tick();check(flash.queued&&flash.ct0==1,"muzzle flash not queued on its first tick");
    Scene scene;scene.background=0xff101010;
    scene.textures.insert(scene.textures.end(),atlas->begin(),atlas->end());
    flash.append(scene,0);
    check(!scene.triangles.empty(),"muzzle flash produced no triangles");
    NJS_MATRIX view;njUnitMatrix(&view);njTranslate(&view,0,0,6);
    SoftwareRenderer image(96,96);image.set_supersampling(1);image.draw_view(scene,view,200,200);
    check(std::count_if(image.pixels().begin(),image.pixels().end(),[](auto pixel){return pixel!=0xff101010;})>0,
          "real muzzle-flash atlas/geometry renders no pixels");
}

// effsub1.c:731 bhEff002 — Rauch/Staub, Texturgruppe 2.
static void puff002(){
    EffectContext c;bool queued=false;
    // Init-Durchlauf zeichnet fuer type<9 nicht (return vor dem UV-Teil).
    auto s=work(2,3);s.mdlver=11;
    check(step_effect_sprite(s,c,&queued)&&!queued&&s.mode0==1,"bhEff002 init must not draw for type<9");
    check(s.tex_id==2&&s.ani_ct==1&&s.bl_src==8&&s.bl_dst==6,"bhEff002 type 3 texture/blend differ");
    for(const auto& v:s.tv)check(v.col==0x40ffffffu,"bhEff002 mdlver 11 colour differs");
    check(s.xn>=-0.05f&&s.xn<=0.05f&&s.yn>=0.1f&&s.yn<=0.2f,"bhEff002 type 3 velocity outside the original rand range");
    const float y0=s.yn;
    check(step_effect_sprite(s,c,&queued)&&queued,"bhEff002 did not enter the translucent list");
    check(close(s.py,y0)&&close(s.yn,y0*0.9f),"bhEff002 type 3 does not move and damp yn");
    check(close(s.sx,4.f*0.0625f)&&close(s.sy,4.f*0.0625f)&&s.ct0==1,"bhEff002 UV tile size/advance differ (uvinfo_v2 row 0)");
    // Typ 0: erzeugt waehrend ct0<4 je Bild eine Typ-10-Arbeit (mdlver 10, lkono=ct0).
    auto trail=work(2,0);step_effect_sprite(trail,c,&queued);take_effect_output();
    step_effect_sprite(trail,c,&queued);auto out=take_effect_output();
    check(out.spawns.size()==1&&out.spawns[0].id==2&&out.spawns[0].type==10&&out.spawns[0].mdlver==10&&out.spawns[0].flg==0x4100001u&&out.spawns[0].lkono==0,"bhEff002 type 0 request differs");
    // Typ 10: ct0=lkono, ct3=128, je Bild -32, bei <=0 Slot frei.
    EffectSpawn spawn=out.spawns[0];EffectSprite fade;initialize_effect_sprite(fade,spawn);
    check(step_effect_sprite(fade,c,&queued)&&queued&&fade.ct3==128,"bhEff002 type 10 must draw in its init pass");
    unsigned frames=0;while(fade.flg&&frames<8){step_effect_sprite(fade,c,&queued);++frames;}
    check(!fade.flg&&frames==4,"bhEff002 type 10 does not release after four fades");
}


// effsub1.c:255 bhEff000: Typ 0 Init-Zufallswerte, Flugbahn, Bodenabprall, CallYakkyouSe(262) nach der zweiten Beruehrung.
static void cartridge_case(){
    std::vector<CollisionWall> walls{{1,6,0,0,0,{-50,0,-50},{100,0,100}}}; // type 6 slope-free floor box would use bhGetGroundPosition; use a type 0 box floor
    walls[0].type=0;walls[0].position={-50,-10,-50};walls[0].extent={100,10,100};
    GroundCollision ground;ground.update(walls,100.f);
    set_effect_walls(&walls,100.f);set_effect_ground(&ground,nullptr,nullptr);set_effect_water({});
    EffectContext c;c.yk_ct=7;bool queued=false;
    { // yk_ct == flr_no releases the work immediately
        EffectSpawn spawn;spawn.id=0;spawn.type=0;spawn.flr_no=7;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(s.flg==0,"bhEff000 must release when yk_ct == flr_no");}
    { // py < -100 releases too
        EffectSpawn spawn;spawn.id=0;spawn.type=0;spawn.flg=1;spawn.position={0,-101,0};EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(s.flg==0,"bhEff000 must release below py -100");}
    recvx_core_random_seed(1);
    EffectSpawn spawn;spawn.id=0;spawn.type=0;spawn.flg=1;spawn.flr_no=3;spawn.position={0,5,0};spawn.ay=4096;EffectSprite s;initialize_effect_sprite(s,spawn);
    recvx_core_random_seed(1);
    const float r1=-recvx_core_random_next()/-2.1474836e9f,r2=-recvx_core_random_next()/-2.1474836e9f,r3=-recvx_core_random_next()/-2.1474836e9f;
    recvx_core_random_seed(1);
    check(step_effect_sprite(s,c,&queued),"bhEff000 not dispatched");
    check(s.tex_id==5&&s.ani_ct==0&&s.polygon_queued&&s.mode0==1,"bhEff000 mode 0 state differs");
    const float zn0=0.65f+0.1f*r2;
    const int ayp=4096-(int(182.04445f*(110.0f+(16.0f*r3-8.0f)))&0xFFFF);
    check(close(s.yn,0.2f*r1-0.04f)&&close(s.zn,zn0)&&s.ayp==ayp,"bhEff000 type 0 launch values differ");
    NJS_POINT3 first{-zn0*njSin(ayp),5+0.2f*r1,-zn0*njCos(ayp)};
    check(close(s.px,first.x)&&close(s.py,first.y)&&close(s.pz,first.z),"bhEff000 first flight step differs");
    check(!(s.flg&0x80),"bhEff000 clears the link bit after init");
    // fall until the floor: mode must leave 1 and the shell sound must be requested at most twice
    unsigned sounds=0;
    for(unsigned pass=0;pass<400&&s.flg;++pass){step_effect_sprite(s,c,&queued);sounds+=unsigned(take_effect_output().room_sounds.size());}
    check(!s.flg,"bhEff000 never released after settling");
    check(sounds>=1&&sounds<=2,"CallYakkyouSe(262) must run for ct0 0 and 1 only");
}
// effsub0.c:136-1106 — Erzeuger 150/152/154/156/158/160/162/164 der Handfassung
// (native/effect_sprite_procedures.cpp): Auftrag aus mode1, unsichtbar, Anforderung
// mit Lage/Winkel/Skalierung des Erzeugers.
static void effsub0_producers(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 150 -> 151 mit type-1; der Auftrag wird verbraucht
        auto s=work(150,0);s.mode1=3;
        check(step_effect_sprite(s,c,&queued),"bhEff150 not dispatched");
        auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==151&&out.spawns[0].type==2&&out.spawns[0].flg==0x4100001u,"bhEff150 request differs");
        check(s.type==0&&s.mode1==0&&(s.flg&0x1000000u)&&!queued,"bhEff150 must consume its order and stay invisible");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff150 must not repeat without a new order");
    }
    { // 156 reicht type durch, 158 bildet 2 -> 1 (sonst 0) ab, 160/162 nehmen type-1
        auto s=work(156,4);step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==157&&out.spawns[0].type==4,"bhEff156 request differs");
        s=work(158,2);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==159&&out.spawns[0].type==1,"bhEff158 type 2 must request row 1");
        s=work(158,5);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].type==0,"bhEff158 other types must request row 0");
        s=work(160,3);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==161&&out.spawns[0].type==2,"bhEff160 request differs");
        s=work(162,1);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==163&&out.spawns[0].type==0,"bhEff162 request differs");
    }
    { // 164: type Explosionen der Reihe 2, eine je Pass
        auto s=work(164,0);s.mode1=3;unsigned spawned=0;
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);for(const auto& e:take_effect_output().spawns){check(e.id==159&&e.type==2,"bhEff164 must request explosion row 2");++spawned;}}
        check(spawned==3&&s.type==0&&s.ct0==0&&s.mode1==0,"bhEff164 must fire exactly type times");
    }
    { // 154: alle sieben Passes ein Funke; die Art folgt rand() >= 0.5 und wechselt immer
        recvx_core_random_seed(5);const float r=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(5);
        auto s=work(154,1);step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==155&&out.spawns[0].type==unsigned(s.ct0),"bhEff154 first spark missing");
        check(s.ct0==(r>=0.5f?2:1)&&s.ct1==6,"bhEff154 first kind must follow rand() >= 0.5 from kind 0");
        for(unsigned pass=0;pass<6;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff154 fired before seven passes");}
        const int before=s.ct0;step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==1&&s.ct0!=before&&s.ct1==6&&s.type==1,"bhEff154 must fire on the seventh pass with another kind and keep its order");
    }
    { // 152: type Blitze entlang (ax,ay) im Abstand sz, Streuung 2*(r1-r2), Startbild ct1 1..11, flr_no = lkono
        EffectSpawn spawn;spawn.id=152;spawn.type=2;spawn.scale={1,1,3};spawn.position={10,0,0};spawn.lkono=7;spawn.flg=1;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        recvx_core_random_seed(9);float r[12];for(auto& v:r)v=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(9);
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==2&&!queued&&(s.flg&0x1000000u),"bhEff152 must request type flashes");
        check(out.spawns[0].id==153&&out.spawns[0].type==1&&out.spawns[1].type==0&&out.spawns[0].flr_no==7&&out.spawns[1].flr_no==7,"bhEff152 start frames/flr_no differ");
        check(close(out.spawns[0].position.x,10+2*(r[0]-r[1]))&&close(out.spawns[0].position.y,2*(r[2]-r[3]))&&close(out.spawns[0].position.z,2*(r[4]-r[5])),"bhEff152 first flash jitter differs");
        check(close(out.spawns[1].position.x,10+2*(r[6]-r[7]))&&close(out.spawns[1].position.z,3+2*(r[10]-r[11])),"bhEff152 second flash must sit sz along the axis");
        check(s.ct1==1&&s.type==2&&s.mode1==0&&close(s.px,10)&&close(s.pz,0),"bhEff152 keeps emitting while type is set and restores its position");
        for(unsigned pass=0;pass<11;++pass){step_effect_sprite(s,c,&queued);out=take_effect_output();}
        check(s.ct1==0&&out.spawns.size()==2&&out.spawns[0].type==0&&out.spawns[1].type==11,"bhEff152 start frame wraps after 11");
    }
}
// effsub0.c:156-1075 — Sprites 151/153/155/157/159/161/163 der Handfassung.
static void effsub0_sprites(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 151: Art 0 faellt mit yn -= 0.02*sz je Pass (Grenze -3), zehn Zeilen, dann frei
        auto s=work(151,0,{1,1,2});float y=0,yn=-0.1f;
        for(unsigned row=0;row<10;++row){
            check(step_effect_sprite(s,c,&queued)&&queued,"bhEff151 frame not queued");
            if(row==0)check(s.tex_id==50&&s.ani_ct==0&&s.bl_src==8&&s.bl_dst==6&&close(s.spd,0.04f)&&s.flg==(1u|0x4100000u),"bhEff151 init differs");
            yn-=0.04f;y+=yn;
            check(close(s.py,y)&&close(s.yn,yn),"bhEff151 fall curve differs");
            check(close(s.tv[0].u,(row%4)*0.25f)&&close(s.tv[0].v,(row/4)*0.25f)&&close(s.sx,1.0f)&&close(s.sy,1.0f),"bhEff151 row UV/size differ");
            for(const auto& v:s.tv)check(v.col==0xffffffffu,"bhEff151 colour set 0 differs");
        }
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff151 must release after the terminator");
        s=work(151,0);step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);s.type=1;step_effect_sprite(s,c,&queued);
        check(s.ani_ct==1&&s.ct0==1&&s.type==1,"bhEff151 kind switch must restart the cursor on the splinter table");
        s=work(151,7);step_effect_sprite(s,c,&queued);check(s.type==0&&s.ani_ct==0,"bhEff151 unknown kinds fall back to the pebble table");
    }
    { // 153: zwei Bilder, Alpha 255 - flr_no*255/20, frei im dritten Pass
        EffectSpawn spawn;spawn.id=153;spawn.type=4;spawn.flr_no=10;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        check(step_effect_sprite(s,c,&queued)&&queued&&s.tex_id==51&&s.mode0==1,"bhEff153 first frame differs");
        check(close(s.tv[0].u,0.625f)&&close(s.tv[0].v,0)&&close(s.sx,4*0.3125f)&&close(s.sy,2.0f),"bhEff153 row 4 differs");
        for(const auto& v:s.tv)check(v.col==((128u<<24)|0xffffffu),"bhEff153 alpha from flr_no differs");
        step_effect_sprite(s,c,&queued);check(queued&&s.mode0==2,"bhEff153 second frame differs");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff153 must release on the third pass");
        spawn.flr_no=20;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);for(const auto& v:s.tv)check(v.col==0xffffffu,"bhEff153 must be invisible from distance 20");
        spawn.type=12;initialize_effect_sprite(s,spawn);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"bhEff153 accepts a row outside uvinfo_geki (bhEff152 only produces 0..11)");
    }
    { // 155: die Art waehlt die Zeilengruppe; sieben Zeilen, dann frei
        auto s=work(155,2);
        check(step_effect_sprite(s,c,&queued)&&queued&&s.tex_id==52&&close(s.tv[0].u,0.75f)&&close(s.tv[0].v,0.375f),"bhEff155 kind 2 first row differs");
        for(unsigned row=1;row<7;++row)step_effect_sprite(s,c,&queued);
        check(queued&&close(s.tv[0].u,0)&&close(s.tv[0].v,0.75f),"bhEff155 kind 2 last row differs");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff155 must release after seven rows");
        s=work(155,1);step_effect_sprite(s,c,&queued);check(close(s.tv[0].u,0.375f)&&close(s.tv[0].v,0.1875f),"bhEff155 kind 1 first row differs");
    }
    { // 157: Vorschub (0,0,sz) um (ax,ay,0) gedreht, zwoelf Zeilen
        EffectSpawn spawn;spawn.id=157;spawn.type=0;spawn.scale={1,1,2};spawn.ax=3000;spawn.ay=16384;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,3000,16384,0);NJS_POINT3 v{0,0,2},d;njCalcVector(&m,&v,&d);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==53&&close(s.px,d.x)&&close(s.py,d.y)&&close(s.pz,d.z)&&close(s.xn,d.x)&&close(s.yn,d.y)&&close(s.zn,d.z),"bhEff157 heading differs");
        for(unsigned row=1;row<12;++row){step_effect_sprite(s,c,&queued);check(queued,"bhEff157 row missing");}
        check(close(s.px,12*d.x)&&close(s.pz,12*d.z),"bhEff157 must advance each pass");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff157 must release after twelve rows");
    }
    { // 159: Reihe 1 (26 Zeilen, Texturbild 2 ab Zeile 14); Art 2 driftet sz*(r-0.5)/8 und wird Art 0
        auto s=work(159,1);
        for(unsigned row=0;row<26;++row){check(step_effect_sprite(s,c,&queued)&&queued,"bhEff159 row missing");check(s.tex_id==54&&s.ani_ct==unsigned(row>=14?2:0),"bhEff159 bank 1 texture differs");}
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff159 row 1 must release after 26 rows");
        recvx_core_random_seed(3);float r[3];for(auto& x:r)x=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(3);
        s=work(159,2,{1,1,4});step_effect_sprite(s,c,&queued);
        check(s.type==0&&close(s.xn,4*(r[0]-0.5f)/8)&&close(s.yn,4*(r[1]-0.5f)/8)&&close(s.zn,4*(r[2]-0.5f)/8),"bhEff159 kind 2 drift differs");
        check(close(s.px,s.xn)&&close(s.py,s.yn)&&close(s.pz,s.zn),"bhEff159 drift applies from the first row");
        for(unsigned row=1;row<24;++row){step_effect_sprite(s,c,&queued);check(queued&&s.ani_ct==unsigned(row>=20?3:row>=18?1:0),"bhEff159 bank 0 texture differs");}
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff159 row 0 must release after 24 rows");
    }
    { // 161: Vorschub (0,0,sz) plus Fall type/500 * ct1; type%10 waehlt die Reihe
        EffectSpawn spawn;spawn.id=161;spawn.type=12;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==55&&s.ani_ct==1&&close(s.spd,12/500.0f)&&s.ct1==1&&close(s.tv[0].u,0)&&close(s.tv[0].v,0.53125f),"bhEff161 kind 2 init differs");
        check(close(s.pz,1)&&close(s.py,0),"bhEff161 first pass has no fall yet");
        step_effect_sprite(s,c,&queued);check(close(s.pz,2)&&close(s.py,-12/500.0f),"bhEff161 second pass falls spd*1");
        step_effect_sprite(s,c,&queued);check(close(s.py,-12/500.0f-2*(12/500.0f)),"bhEff161 third pass falls spd*2");
        for(unsigned row=3;row<10;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff161 row 9 missing");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff161 kind 2 must release after ten rows");
        s=work(161,0);step_effect_sprite(s,c,&queued);check(s.ani_ct==0&&close(s.spd,0)&&close(s.tv[0].u,0.625f),"bhEff161 kind 0 init differs");
    }
    { // 163: Art 0 steigt (0,0.5,0) bis Zeile 13 und wiederholt Zeile 6..11 int(sz) mal (-3);
      // Art 1 liegt (Eckpunkte um (0,ay,-ax) gedreht), Leerzeilen -4, Schleife -2
        auto s=work(163,0,{1,1,3});unsigned drawn=0,passes=0;
        while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);if(queued)++drawn;++passes;}
        check(!s.flg&&drawn==12+2*6+4&&passes==drawn+1,"bhEff163 kind 0 repeat/exit sequence differs");
        check(close(s.py,25*0.5f),"bhEff163 kind 0 rises on every pass that starts before row 13 (12 rows, twice 1+5 repeat passes, the final -3 pass)");
        EffectSpawn spawn;spawn.id=163;spawn.type=1;spawn.ax=16384;spawn.flg=1;initialize_effect_sprite(s,spawn);
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,0,0,-16384);NJS_POINT3 corner{-1,-1,0},d;njCalcVector(&m,&corner,&d);
        step_effect_sprite(s,c,&queued);check(queued&&s.ct0==1&&close(s.tv[0].x,d.x)&&close(s.tv[0].y,d.y)&&close(s.tv[0].z,d.z)&&close(s.px,0),"bhEff163 kind 1 first row/corners differ");
        for(unsigned row=1;row<8;++row)step_effect_sprite(s,c,&queued);check(queued&&s.ct0==8,"bhEff163 kind 1 eight rows");
        for(unsigned blank=0;blank<5;++blank){step_effect_sprite(s,c,&queued);check(!queued,"bhEff163 -4 rows must not draw");}
        check(s.ct0==13,"bhEff163 blank cursor");
        step_effect_sprite(s,c,&queued);check(queued&&s.ct0==1&&close(s.tv[0].u,0)&&close(s.tv[0].v,0.1875f),"bhEff163 -2 row must loop to row 0");
    }
}
// effsub0.c:1216-3223 — Handfassung 166..194 (native/effect_sprite_procedures.cpp).
static void effsub0_batch_b(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 166: alle 13 Passes ein 167 der Art 1; Auftrag aus dem niedrigen Byte von mode1, bleibt stehen
        auto s=work(166,0);s.mode1=0x105;unsigned spawned=0;
        for(unsigned pass=0;pass<27;++pass){step_effect_sprite(s,c,&queued);for(const auto& e:take_effect_output().spawns){check(e.id==167&&e.type==1,"bhEff166 request differs");++spawned;}}
        check(spawned==3&&s.type==5&&s.mode1==0,"bhEff166 must fire on passes 0, 13 and 26 and keep its order");
    }
    { // 167/169: Eckpunkte um (0,ay,-ax) gedreht; 169 Art 2 spiegelt x
        EffectSpawn spawn;spawn.id=167;spawn.ax=16384;spawn.ay=8192;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,0,8192,-16384);NJS_POINT3 corner{1,-1,0},d,mirrored{-1,-1,0},dm;njCalcVector(&m,&corner,&d);njCalcVector(&m,&mirrored,&dm);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==57&&close(s.tv[1].x,d.x)&&close(s.tv[1].y,d.y)&&close(s.tv[1].z,d.z),"bhEff167 corners differ");
        for(unsigned row=1;row<13;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff167 row 12 missing");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff167 must release after 13 rows");
        spawn.id=169;spawn.type=2;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==64&&s.type==2&&close(s.tv[1].x,dm.x)&&close(s.tv[1].y,dm.y)&&close(s.tv[0].u,0)&&close(s.tv[0].v,0.375f),"bhEff169 kind 2 must mirror the corners on the long row");
        spawn.type=7;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);check(s.type==0&&close(s.tv[0].v,0),"bhEff169 unknown kinds use the short row as kind 0");
    }
    { // 171: die Zehnzeilen-Reihe laeuft type-mal
        auto s=work(171,2);unsigned drawn=0;while(s.flg&&drawn<100){step_effect_sprite(s,c,&queued);if(queued)++drawn;}
        check(!s.flg&&drawn==20&&s.tex_id==55,"bhEff171 must run its row twice for type 2");
    }
    { // 173: Blendpaar jeden Pass aus type, Vollkachel ohne Ende, Grundskalierung 1
        auto s=work(173,25,{3,3,3});step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==70&&s.bl_src==4&&s.bl_dst==7&&close(s.sx,4*0.99609375f)&&close(s.sxb,1),"bhEff173 blend/size differ");
        s.type=93;step_effect_sprite(s,c,&queued);check(queued&&s.bl_src==11&&s.bl_dst==5,"bhEff173 blend pair must follow type every pass");
        step_effect_sprite(s,c,&queued);check(queued&&s.flg!=0,"bhEff173 must not end");
        s.type=100;bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff173 accepts a type outside bltbl");
    }
    { // 175/176: Mond, eine Kachel ohne Ende
        auto s=work(175,2);step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==176&&out.spawns[0].type==0,"bhEff175 request differs");
        s=work(176,0);for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==68&&s.ct0==0&&close(s.sx,4)&&close(s.tv[3].u,1)&&close(s.tv[3].v,1),"bhEff176 must draw the full tile every pass");}
    }
    { // 178: startet bei Zeile type, fliegt (0,0,sz) je Pass, lebt lkono Passes, Zeilen laufen um
        EffectSpawn spawn;spawn.id=178;spawn.type=4;spawn.scale={1,1,2};spawn.lkono=9;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==69&&s.ct1==1&&close(s.pz,2)&&close(s.tv[0].u,0.3125f)&&close(s.tv[0].v,0.34375f),"bhEff178 first pass differs");
        step_effect_sprite(s,c,&queued);check(close(s.tv[0].u,0.625f),"bhEff178 row 5");
        step_effect_sprite(s,c,&queued);check(queued&&close(s.tv[0].u,0)&&close(s.tv[0].v,0)&&close(s.pz,6),"bhEff178 rows must wrap to 0");
        for(unsigned pass=3;pass<8;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.flg,"bhEff178 pass 8 still alive");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff178 must release once ct1 reaches lkono");
        spawn.type=6;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);check(s.ct0==1&&close(s.tv[0].u,0),"bhEff178 kinds from 6 start at row 0");
    }
    { // 179/180: sz 0 feuert einmal, sonst alle int(sz) Passes
        auto s=work(179,2,{1,1,0});step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==180&&out.spawns[0].type==1&&s.type==0,"bhEff179 sz 0 must fire once");
        s=work(179,1,{1,1,3});unsigned spawned=0;for(unsigned pass=0;pass<7;++pass){step_effect_sprite(s,c,&queued);spawned+=unsigned(take_effect_output().spawns.size());}
        check(spawned==3&&s.type==1,"bhEff179 must fire on passes 0, 3 and 6");
        s=work(180,1);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==408&&s.ani_ct==1&&close(s.tv[0].v,0.3125f),"bhEff180 kind 1 differs");
        for(unsigned row=1;row<10;++row)step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff180 must release after ten rows");
    }
    { // 183/184: jeden Pass ein Tropfen; der Tropfen faellt, streut, landet auf Hoehe type und spritzt vier Zeilen vierfach gross
        auto s=work(183,3);for(unsigned pass=0;pass<3;++pass){step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==184&&out.spawns[0].type==2,"bhEff183 must request a drop every pass");}
        check(s.type==3,"bhEff183 keeps its order");
        EffectSpawn spawn;spawn.id=184;spawn.type=1;spawn.scale={1,1,5};spawn.position={0,3,0};spawn.flg=1;initialize_effect_sprite(s,spawn);
        recvx_core_random_seed(4);float r[6];for(auto& x:r)x=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(4);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==33&&close(s.zn,0.5f)&&close(s.px,(r[0]-r[1])/16)&&close(s.py,3+(r[2]-r[3])/16-0.01f)&&close(s.pz,0.5f+(r[4]-r[5])/16)&&close(s.spd,0.02f)&&s.ct1==99,"bhEff184 first fall step differs");
        unsigned passes=1;while(s.ct0<11&&passes<400){step_effect_sprite(s,c,&queued);++passes;}
        check(s.ct0==12&&queued&&close(s.py,1)&&close(s.sxb,4)&&close(s.syb,4)&&close(s.sx,4),"bhEff184 must land at height type and grow four times");
        unsigned more=0;for(;s.flg&&more<10;++more)step_effect_sprite(s,c,&queued);
        check(more==4&&!s.flg,"bhEff184 splash must end after rows 12..14");
    }
    { // 185: Tastenleuchte, vier Bilder der gewaehlten Taste im Kreis; andere Typen zeichnen nichts
        auto s=work(185,3);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==410&&(s.flg&0x4000000u)&&s.bl_src==8&&s.bl_dst==6&&s.ct0==1&&close(s.tv[0].v,0.3125f)&&close(s.tv[0].u,0)&&close(s.tv[1].u,0.234375f),"bhEff185 first frame differs");
        check(s.tv[0].x==-1&&s.tv[0].y==-1&&s.sx==1,"bhEff185 must not touch the corners or size");
        step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(queued&&s.ct0==0&&close(s.tv[0].u,0.234375f),"bhEff185 fourth frame repeats the second column and wraps");
        s.type=0;step_effect_sprite(s,c,&queued);check(!queued&&s.flg,"bhEff185 type 0 draws nothing but stays");
    }
    { // 187: Art 2 einmal Reihe 1; Art 3 Reihe 0 mit Pause interval0[10*r]; sonst einmal Reihe 0
        auto s=work(187,2);step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==188&&out.spawns[0].type==1&&s.type==0,"bhEff187 kind 2 differs");
        s=work(187,7);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].type==0&&s.type==0,"bhEff187 default differs");
        recvx_core_random_seed(6);const float r=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(6);
        s=work(187,3);step_effect_sprite(s,c,&queued);out=take_effect_output();
        const int interval=17+5*int(10*r);
        check(out.spawns.size()==1&&out.spawns[0].type==0&&s.ct1==interval-1&&s.type==3,"bhEff187 kind 3 interval differs");
        for(int pass=1;pass<interval;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff187 fired inside the pause");}
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff187 must fire again after the pause");
    }
    { // 188: zwoelf beziehungsweise fuenf Zeilen
        auto s=work(188,0);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==412&&close(s.tv[0].u,0),"bhEff188 row 0");for(unsigned row=1;row<12;++row)step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff188 kind 0 must release after twelve rows");
        s=work(188,1);for(unsigned row=0;row<5;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff188 kind 1 row 4");step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff188 kind 1 must release after five rows");
    }
    { // 190/192/194: sichtbar nur mit Auftrag; bhSetEffectTb-Anforderungen mit flg 1 und Link-Nummer
        auto s=work(190,0);step_effect_sprite(s,c,&queued);check((s.flg&0x1000000u)&&take_effect_output().spawns.empty(),"bhEff190 idle must be invisible");
        s.mode1=1;step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(!(s.flg&0x1000000u)&&out.spawns.size()==1&&out.spawns[0].id==191&&out.spawns[0].flg==1&&out.spawns[0].lkono==0xff&&s.ct0==0,"bhEff190 first order must fire at once");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff190 fired early");}
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff190 must fire every six passes");
        EffectSpawn spawn;spawn.id=192;spawn.lkono=3;spawn.flg=1;initialize_effect_sprite(s,spawn);s.mode1=2;step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==3&&out.spawns[0].id==193&&out.spawns[2].lkono==0xff&&s.type==0&&s.mode1==0,"bhEff192 must request lkono copies once");
        s=work(194,5);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==195&&out.spawns[0].type==1&&out.spawns[0].lkono==0&&s.type==0,"bhEff194 kind (type-1)%3 differs");
    }
}
// effsub1.c / effsub5.c / effsub6.c — Handfassung 8/10/11/14/15/29/101/103/104/116/117/119/
// 121/128/131/132/144/206/208/214/215/222/224/231/241 (native/effect_sprite_procedures.cpp).
static void effsub1_batch(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 008: Art 2 laeuft die Startreihe und wechselt dann auf f1 als Art 0; Quad -1..1 x -2..0
        auto s=work(8,2);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==8&&s.tv[0].y==-2&&s.tv[2].y==0&&s.tv[0].col==0xffe0e0e0u&&close(s.tv[0].v,0.375f),"bhEff008 start row differs");
        for(unsigned row=1;row<11;++row)step_effect_sprite(s,c,&queued);
        check(queued&&s.type==0&&s.ct0==1&&close(s.tv[0].v,0.875f),"bhEff008 must switch to row f1 as kind 0 after the start row");
    }
    { // 010: Art 0 steigt 0.8 und springt an der Reihengrenze zurueck; Art 2 endet dort, Blend 8/6
        EffectSpawn spawn;spawn.id=10;spawn.position={0,5,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==19&&s.bl_src==11&&s.bl_dst==3&&close(s.py,5.8f)&&close(s.yn,5),"bhEff010 first pass differs");
        for(unsigned row=1;row<4;++row)step_effect_sprite(s,c,&queued);check(close(s.py,5+4*0.8f),"bhEff010 rises 0.8 per pass");
        step_effect_sprite(s,c,&queued);check(queued&&s.flg&&close(s.py,5.8f)&&s.ct0==1,"bhEff010 kind 0 must restart at its start height");
        spawn.type=2;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);check(s.bl_src==8&&s.bl_dst==6&&close(s.py,5),"bhEff010 kind 2 does not rise");
        for(unsigned row=1;row<4;++row)step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff010 kind 2 must end after its rows");
    }
    { // 011: mdlver 0 Art 1 wirft 0.8 (lkono < 2) und driftet gegen ay; lkono 1 spiegelt; mdlver 1 flr_no 0 nutzt den Versatzverfall 0.7
        EffectSpawn spawn;spawn.id=11;spawn.type=1;spawn.ay=16384;spawn.lkono=1;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.flg==0x4100001u&&s.tex_id==34&&s.ani_ct==1&&s.bl_dst==10&&s.tv[0].x==1&&s.tv[1].x==-1&&s.tv[0].col==0xffffffffu,"bhEff011 mirrored init differs");
        check(close(s.px,-0.5f*njSin(16384))&&close(s.py,0.8f)&&close(s.yn,0.65f)&&close(s.xn,0.03f)&&close(s.sx,8*0.21875f),"bhEff011 first throw step differs");
        step_effect_sprite(s,c,&queued);check(close(s.px,-0.5f*njSin(16384)-0.03f*njSin(16384))&&close(s.py,0.8f+0.65f),"bhEff011 drifts xn against ay");
        spawn.mdlver=1;spawn.type=5;spawn.lkono=0;spawn.has_offset=true;spawn.offset={1,0,0};initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==2&&s.ani_ct==1&&s.bl_dst==10&&s.tv[0].col==0xc0ffdcc0u&&close(s.px,1)&&close(s.lox,0.7f),"bhEff011 mdlver 1 smoke/offset decay differs");
        spawn.type=12;spawn.mdlver=0;initialize_effect_sprite(s,spawn);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff011 accepts a kind without an original table");
    }
    { // 014: Art 3 stehende Scheibe, Alpha ct1 200 -4 je Pass bis 0; Art 2 Versatzverfall 0.8
        auto s=work(14,3);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==3&&s.bl_dst==10&&s.tv[0].x==0&&s.tv[0].z==-1&&s.tv[0].col==((200u<<24)|0xffffffu)&&s.ct1==196&&s.ct0==0,"bhEff014 kind 3 first pass differs");
        unsigned passes=1;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}check(passes==51&&!s.flg,"bhEff014 kind 3 must end once ct1 reaches 0");
        EffectSpawn spawn;spawn.id=14;spawn.type=2;spawn.has_offset=true;spawn.offset={0,2,0};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&close(s.py,2)&&close(s.loy,1.6f)&&s.ani_ct==0&&close(s.sx,8*0.0625f),"bhEff014 kind 2 offset decay differs");
    }
    { // 015: Art 0 waechst, fliegt, faellt und erzeugt am Ende Art 1 (Groesse 2, Lage ±1); Art 2 wartet flr_no Passes unsichtbar
        recvx_core_random_seed(8);float r[2];for(auto& x:r)x=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(8);
        auto s=work(15,0);step_effect_sprite(s,c,&queued);
        check(queued&&s.flg==0x4100001u&&s.tex_id==33&&s.tv[0].col==0xa0ffffffu&&close(s.sx,1.1f)&&close(s.py,0.2f)&&close(s.xn,0.7f-0.049f)&&close(s.yn,0.05f),"bhEff015 kind 0 first pass differs");
        for(unsigned row=1;row<10;++row)step_effect_sprite(s,c,&queued);check(queued&&s.flg,"bhEff015 kind 0 row 9");
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(!queued&&s.flg==0&&out.spawns.size()==1&&out.spawns[0].id==15&&out.spawns[0].type==1&&close(out.spawns[0].scale.x,2)&&close(out.spawns[0].position.x,s.px+2*r[0]-1)&&close(out.spawns[0].position.z,s.pz+2*r[1]-1)&&out.spawns[0].ay==0,"bhEff015 kind 0 must end with a kind-1 puddle");
        EffectSpawn spawn;spawn.id=15;spawn.type=2;spawn.flr_no=2;spawn.flg=1;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(!queued&&(s.flg&0x1000000u)&&s.flr_no==1&&s.tex_id==34&&s.tv[0].y==-2,"bhEff015 kind 2 waits invisibly");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flr_no==0,"bhEff015 kind 2 second wait");
        step_effect_sprite(s,c,&queued);check(queued&&!(s.flg&0x1000000u)&&close(s.sx,8*0.125f),"bhEff015 kind 2 shows after flr_no passes");
    }
    { // 029: Freigabe nach 32 Passes (Attributbit setzt der Raumbesitzer)
        auto s=work(29,0);for(unsigned pass=0;pass<31;++pass){step_effect_sprite(s,c,&queued);check(s.flg,"bhEff029 released early");}step_effect_sprite(s,c,&queued);check(s.flg==0&&s.ct0==0,"bhEff029 must release on pass 32");
    }
    { // 101/103/104/116: unsichtbare Erzeuger ueber die gemeinsame Vorlage sys->ef
        auto s=work(101,0,{2,2,2});step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==107&&out.spawns[0].flg==1&&close(out.spawns[0].scale.x,2)&&close(out.spawns[0].position.x,0)&&(s.flg&0x1000000u),"bhEff101 request differs");
        s.stflg|=0x1000000u;step_effect_sprite(s,c,&queued);check(s.flg==0&&take_effect_output().spawns.empty(),"bhEff101 must stop once stflg 0x1000000 is set");
        s=work(103,1);for(unsigned pass=0;pass<3;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff103 kind 1 fired early");}
        step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==119&&out.spawns[0].type==0&&out.spawns[0].ay==int(2048.0f*njSin(0))&&s.ct1==2048,"bhEff103 kind 1 must fire every fourth pass with the swinging angle");
        s=work(103,2);step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff103 kind 2 first pass");step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].type==1&&out.spawns[0].ay==0,"bhEff103 kind 2 fires every second pass");
        recvx_core_random_seed(12);const float gate=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(12);
        s=work(103,0);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==(gate<0.3f?1u:0u),"bhEff103 kind 0 must follow rand() < 0.3");
        EffectSpawn spawn;spawn.id=104;spawn.type=2;spawn.flr_no=-1;spawn.mdlver=1;spawn.flg=1;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==2&&out.spawns[0].id==12&&out.spawns[0].type==1&&out.spawns[0].flg==0x100001u&&close(out.spawns[1].scale.x,0.1f)&&s.flg,"bhEff104 first pass differs");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==2&&s.flg==0,"bhEff104 must end after mdlver+1 passes");
        s=work(116,0);unsigned fired=0;for(unsigned pass=0;pass<8;++pass){step_effect_sprite(s,c,&queued);for(const auto& e:take_effect_output().spawns){check(e.id==15&&e.type==5&&e.mdlver==0,"bhEff116 request differs");++fired;}}
        check(fired==2,"bhEff116 must fire every fourth pass");
    }
    { // 117/121: Schleifenbilder; 117 endet ueber mode0 2
        recvx_core_random_seed(2);const float r=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(2);
        auto s=work(117,1);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==38&&s.ct1==int(4*r)+1&&close(s.tv[0].u,0.5625f),"bhEff117 init differs");
        for(unsigned pass=0;pass<3;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.ct0==1&&close(s.tv[0].u,0.5625f),"bhEff117 must loop its three frames");
        s.mode0=2;step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff117 mode0 2 must release");
        s=work(121,0);for(unsigned pass=0;pass<9;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==40&&(s.flg&0x4000000u)&&s.ct0==1&&s.tv[2].y==-2&&close(s.tv[0].u,0),"bhEff121 must loop eight frames");
    }
    { // 119: Art 0 steigt 0.5, Alpha 192 -8, Drift gegen ay mit 0.1*sz*0.9; Art 1 schwingt um gpy und driftet xn/zn
        recvx_core_random_seed(21);float r[2];for(auto& x:r)x=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(21);
        EffectSpawn spawn;spawn.id=119;spawn.type=0;spawn.scale={1,1,2};spawn.position={0,1,0};spawn.ay=16384;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==39&&s.bl_src==11&&s.bl_dst==3&&s.tv[0].col==((192u<<24)|0xffffffu)&&s.ct2==184&&close(s.py,1.5f)&&close(s.spd,0.18f)&&close(s.px,-0.18f*njSin(16384))&&close(s.xn,0.4f*r[0]-0.2f)&&close(s.zn,0.4f*r[1]-0.2f),"bhEff119 kind 0 first pass differs");
        for(unsigned row=1;row<15;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff119 kind 0 row 14");step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff119 kind 0 must end after 15 rows");
        spawn.type=1;initialize_effect_sprite(s,spawn);recvx_core_random_seed(21);step_effect_sprite(s,c,&queued);
        check(queued&&close(s.px,0.4f*r[0]-0.2f)&&close(s.py,1+2*njSin(0))&&close(s.sx,8*0.1875f)&&close(s.tv[0].u,0.40625f),"bhEff119 kind 1 starts at row 5 and swings");
        step_effect_sprite(s,c,&queued);check(close(s.py,1+2*njSin(1024)),"bhEff119 kind 1 swing uses ct0*1024");
        for(unsigned row=2;row<10;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff119 kind 1 row 9");step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff119 kind 1 must end after ten rows");
    }
    { // 128: Art 0 waechst 0.1*sx und blendet ueber ct1; sonst dreht sy Grad und pulsiert bis ct0 ueberlaeuft; mdlver waehlt die Farbe
        EffectSpawn spawn;spawn.id=128;spawn.type=0;spawn.scale={2,1,2};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==2&&s.ani_ct==1&&s.tv[0].y==0&&s.tv[0].z==-1&&close(s.sx,2.2f)&&close(s.sz,2.2f)&&s.tv[0].col==((188u<<24)|0xc8b440u),"bhEff128 kind 0 first pass differs");
        unsigned passes=1;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}check(passes==48&&!s.flg,"bhEff128 kind 0 must end once ct1 reaches 0");
        spawn.type=1;spawn.flr_no=3;spawn.mdlver=1;spawn.scale={1,45,1};initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==43&&close(s.tv[0].u,0.5f)&&close(s.tv[0].v,0.5f)&&s.ay==(int(182.04445f*45)&0xffff)&&s.ct0==512&&s.ct1==0&&s.tv[0].col==0xffffffu,"bhEff128 quarter/rotation/pulse differ");
        step_effect_sprite(s,c,&queued);check(s.ct1==int(160.0f*njSin(512)),"bhEff128 pulse amplitude differs");
        passes=2;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}check(passes==64&&!s.flg,"bhEff128 must end when ct0 wraps at 0x8000");
        spawn.mdlver=2;initialize_effect_sprite(s,spawn);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff128 accepts a mdlver without an original colour");
    }
    { // 131/132: Versatzflug mit Verfall; 132 Art 3 erzeugt in den Passes 1..2 Kopien mit Versatz (xn,yn,zn)
        EffectSpawn spawn;spawn.id=131;spawn.has_offset=true;spawn.offset={1,0,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==2&&s.tv[0].col==0x8010c040u&&close(s.px,1)&&close(s.lox,0.9f)&&close(s.sx,4*0.0625f),"bhEff131 first pass differs");
        for(unsigned row=1;row<13;++row)step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff131 must end after 13 rows");
        spawn.id=132;spawn.type=3;spawn.offset={0.5f,0.25f,0.125f};spawn.position={1,2,3};spawn.scale={2,2,2};initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(queued&&out.spawns.empty()&&s.tex_id==19&&s.bl_src==9&&s.tv[0].col==((208u<<24)|0xffffffu)&&close(s.sxb,2.2f)&&close(s.px,1.5f),"bhEff132 kind 3 first pass differs");
        step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==132&&out.spawns[0].type==3&&out.spawns[0].mdlver==1&&out.spawns[0].has_offset&&close(out.spawns[0].offset.x,0.5f)&&close(out.spawns[0].offset.y,0.25f)&&close(out.spawns[0].offset.z,0.125f)&&close(out.spawns[0].position.x,1)&&close(out.spawns[0].scale.x,2.2f*0.9f)&&close(out.spawns[0].scale.z,1)&&close(s.spd,0.8f),"bhEff132 kind 3 copy must carry the (xn,yn,zn) offset and the origin");
        step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&close(out.spawns[0].scale.x,2.2f*1.1f*0.8f),"bhEff132 kind 3 second copy");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff132 kind 3 stops copying after pass 2");
        spawn.type=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==10&&s.ani_ct==2&&close(s.tv[0].u,0.875f)&&close(s.sx,1.6f),"bhEff132 kind 1 shrinks");
        unsigned passes=1;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}check(passes==14&&!s.flg,"bhEff132 kind 1 must end below size 0.1");
    }
    { // 144: jeden Pass gezeichnet, ct1 1..5 dann 0, Texturbild mdlver
        EffectSpawn spawn;spawn.id=144;spawn.mdlver=3;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        for(unsigned pass=0;pass<7;++pass){step_effect_sprite(s,c,&queued);check(queued&&s.ani_ct==3&&s.ct1==int(pass==0||pass==6?0u:pass),"bhEff144 cycle differs");}
    }
    { // effsub5: 206/208 ab dem zweiten Pass alle sechs Passes; 214/215/224 einmal; 222 alle lkono Passes mit Streuung; 231 Takt int(sz); 241 lkono Kopien
        auto s=work(206,3);step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&!(s.flg&0x1000000u),"bhEff206 first pass only arms");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff206 fired early");}
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==207&&out.spawns[0].type==2&&out.spawns[0].lkono==0xff&&out.spawns[0].flg==1,"bhEff206 request differs");
        s=work(208,1);for(unsigned pass=0;pass<7;++pass){step_effect_sprite(s,c,&queued);out=take_effect_output();}check(out.spawns.size()==1&&out.spawns[0].id==209&&out.spawns[0].type==0,"bhEff208 request differs");
        s=work(214,4);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==11&&out.spawns[0].type==0&&out.spawns[0].lkono==0xff&&s.type==0,"bhEff214 request differs");
        s=work(215,8);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==217&&out.spawns[0].type==1&&s.type==0,"bhEff215 kinds 6..8 go to 217");
        s=work(215,3);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==216&&out.spawns[0].type==2,"bhEff215 kinds 0..5 go to 216");
        EffectSpawn spawn;spawn.id=222;spawn.type=6;spawn.lkono=3;spawn.scale={1,1,0};spawn.position={1,2,3};spawn.flg=1;EffectSprite t;initialize_effect_sprite(t,spawn);
        recvx_core_random_seed(5);float r[2];for(auto& x:r)x=-recvx_core_random_next()/-2.1474836e9f;recvx_core_random_seed(5);
        step_effect_sprite(t,c,&queued);check(take_effect_output().spawns.empty()&&close(t.sz,0.1f)&&t.ct0==1,"bhEff222 pass 1");step_effect_sprite(t,c,&queued);check(take_effect_output().spawns.empty(),"bhEff222 pass 2");
        step_effect_sprite(t,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==223&&out.spawns[0].type==1&&close(out.spawns[0].position.y,2+6*r[0]-3)&&close(out.spawns[0].position.z,3+6*r[1]-3)&&close(out.spawns[0].scale.z,0.01f)&&t.ct0==0,"bhEff222 request differs");
        s=work(224,6);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==225&&out.spawns[0].type==1&&out.spawns[0].lkono==0,"bhEff224 request differs");
        s=work(231,2,{1,1,2});unsigned fired=0;for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);for(const auto& e:take_effect_output().spawns){check(e.id==232&&e.type==1&&close(e.scale.z,1),"bhEff231 request differs");++fired;}}
        check(fired==3&&s.type==2,"bhEff231 must fire on passes 0, 2 and 4");
        s=work(231,1,{1,1,0});step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1&&s.type==0,"bhEff231 sz 0 fires once");
        spawn=EffectSpawn{};spawn.id=241;spawn.type=4;spawn.lkono=2;spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==2&&out.spawns[1].id==242&&out.spawns[1].type==3&&s.type==0&&s.mode0==0,"bhEff241 must request lkono copies once");
    }
}
static int eff223_u0(int ct2){static const int u[18]={0,40,80,120,160,200,0,40,80,120,160,200,0,40,80,120,160,200};return u[ct2%18];}
// Gruppe 2 (effsub0 Rest, effsub1 018/124, effsub5, effsub6): Erzeugernachtraege am neuen
// eff[n] (Startgeschwindigkeit, tex_id, az, lkono, Elternsatz), Opak-Liste, Kamerawinkel.
static float frac(){return -recvx_core_random_next()/-2.1474836e9f;}
static void install_group4_services();
static void group2_producers(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 177: eff[n].lkono = op->lkono
        EffectSpawn spawn;spawn.id=177;spawn.type=3;spawn.lkono=9;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==178&&out.spawns[0].type==2&&out.spawns[0].lkono==9&&s.type==0,"bhEff177 must hand its lkono to the child");
        EffectSprite child;initialize_effect_sprite(child,out.spawns[0]);check(child.lkono==9,"child lkono");
    }
    { // 238: Kind 239 mit Startgeschwindigkeit (0,0,-sz/4*1.2/10) um (ay,ax); 0xFF einmalig, sonst alle type Passes
        EffectSpawn spawn;spawn.id=238;spawn.type=2;spawn.scale={1,1,4};spawn.ay=16384;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==239&&out.spawns[0].has_velocity&&close(out.spawns[0].scale.z,1)&&s.mode0==2,"bhEff238 first child missing");
        NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,16384);njRotateX(&m,0);NJS_POINT3 v{0,0,float((((double)-4.0/4.0)*1.2)/10.0)},d;njCalcVector(&m,&v,&d);
        check(close(out.spawns[0].velocity.x,d.x)&&close(out.spawns[0].velocity.z,d.z),"bhEff238 child velocity differs");
        EffectSprite child;initialize_effect_sprite(child,out.spawns[0]);check(close(child.xn,d.x)&&close(child.zn,d.z),"velocity patch must reach the new work");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.ct0==1,"bhEff238 pause pass 1");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.mode0==1&&s.ct0==0,"bhEff238 pause pass 2 re-arms");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff238 fires again after type passes");
        s.type=0;s.mode1=0;step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.mode0==0&&(s.flg&0x1000000u),"bhEff238 idle resets mode0");
        s.mode1=0xff;step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&s.type==0&&s.mode0==0,"bhEff238 0xFF is a one-shot");
        // Verlinkt (flg 0x80 aus bhSetEffectLink): Drehanteil der Linkmatrix, dann (ay, ax).
        s.flg|=0x80u;s.mode1=1;s.mode0=0;s.type=0;
        NJS_MATRIX joint;njUnitMatrix(&joint);njTranslate(&joint,50,60,70);njRotateZ(&joint,8192);njRotateX(&joint,4096);
        s.link_matrix=joint;step_effect_sprite(s,c,&queued);s.link_matrix=nullptr;out=take_effect_output();
        {NJS_MATRIX r;std::copy(joint,joint+16,r);njUnitTransPortion(&r);njRotateY(&r,s.ay);njRotateX(&r,s.ax);NJS_POINT3 e;njCalcPoint(&r,&v,&e);
         check(out.spawns.size()==1&&close(out.spawns[0].velocity.x,e.x)&&close(out.spawns[0].velocity.y,e.y)&&close(out.spawns[0].velocity.z,e.z),"bhEff238 linked child must turn with the joint's rotation");}
        // Ohne native Gelenkmatrix: Luecke, kein Kind mit geratener Richtung.
        s.mode0=1;step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.empty()&&out.gaps.size()==1,"bhEff238 on a linked work without the node matrix reports the gap");
        bool rejected=false;
    }
    { // 228: 229 mit az 8192 (gemeldet, nicht portiert) plus Rauch 2 Art 6 vor dem Erzeuger
        EffectSpawn spawn;spawn.id=228;spawn.type=1;spawn.lkono=2;spawn.scale={4,4,4};spawn.position={1,0,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff228 pass 1 waits");
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==2&&out.spawns[0].id==229&&out.spawns[0].az==8192&&out.spawns[0].mdlver==1&&close(out.spawns[0].scale.x,3)&&out.spawns[0].flr_no==2,"bhEff228 first request differs");
        check(out.spawns[1].id==2&&out.spawns[1].type==6&&out.spawns[1].flg==0x4100001u&&close(out.spawns[1].position.z,1.5f)&&close(out.spawns[1].position.x,1)&&close(out.spawns[1].scale.x,2.5f)&&s.ct0==0&&s.ct1==1,"bhEff228 smoke request differs");
        EffectSprite child;initialize_effect_sprite(child,out.spawns[0]);check(child.az==8192,"az patch must reach the new work");
    }
    { // 245: Art (type-1)/2, Bild (type-1)%2, lkono weitergereicht; Art 1 nur alle lkono Passes
        auto s=work(245,2);s.lkono=5;step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==246&&out.spawns[0].type==0&&out.spawns[0].mdlver==1&&out.spawns[0].lkono==5,"bhEff245 kind 0 differs");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff245 kind 0 fires every pass");
        s=work(245,3);s.lkono=3;step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].type==1&&out.spawns[0].mdlver==0&&s.mode0==1,"bhEff245 kind 1 first pass fires");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff245 kind 1 pass 2 waits");step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff245 kind 1 pass 3 waits");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1,"bhEff245 kind 1 fires after lkono passes");
    }
    { // 247/226/202: eff[n].exp0 = op -> parent_slot = eigener Platz
        auto s=work(247,3);s.slot=17;step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==248&&out.spawns[0].type==2&&out.spawns[0].parent_slot==17,"bhEff247 must link the child to its slot");
        EffectSprite child;initialize_effect_sprite(child,out.spawns[0]);check(child.parent_slot==17,"parent_slot must reach the new work");
        s=work(226,1,{3,1,1});s.slot=4;recvx_core_random_seed(3);step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==8&&out.spawns[7].id==227&&out.spawns[7].mdlver==7&&out.spawns[7].parent_slot==4&&close(out.spawns[0].scale.x,3)&&close(out.spawns[0].scale.y,3)&&close(out.spawns[0].scale.z,1)&&s.mode0==1&&s.mode1==0,"bhEff226 ring differs");
        for(const auto& e:out.spawns)check(e.type<2,"bhEff226 child kinds are 0/1");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff226 fires once");
        EffectSpawn spawn;spawn.id=202;spawn.type=4;spawn.lkono=2;spawn.scale={2,3,5};spawn.position={1,2,3};spawn.ax=100;spawn.ay=200;spawn.flg=1;initialize_effect_sprite(s,spawn);s.slot=9;
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&(s.flg&0x1000000u)&&s.ct1==1024&&s.ct2==64&&close(s.spd,0),"bhEff202 pass 1 only swings");
        recvx_core_random_seed(7);float r[5];for(auto& x:r)x=frac();recvx_core_random_seed(7);
        step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==203&&out.spawns[0].type==2&&out.spawns[0].parent_slot==9&&out.spawns[0].mdlver==(unsigned(4*r[0])&0xff)&&close(std::fabs(out.spawns[0].scale.x),2+(2*r[1])/4)&&close(std::fabs(out.spawns[0].scale.y),2+(2*r[2])/4)&&close(out.spawns[0].scale.z,0.5f)&&out.spawns[0].ax==100&&out.spawns[0].ay==200&&close(out.spawns[0].position.y,2)&&s.ct0==0,"bhEff202 kind 4 child differs");
        check(close(s.spd,2.5f*njSin(1024)+6.0f*njSin(64)),"bhEff202 swing differs");
        s.type=1;s.ct0=1;step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&s.ax==0&&s.ay==16384&&out.spawns[0].ax>=-3640&&out.spawns[0].ax<3641,"bhEff202 kind 1 turns the producer up and scatters the child angles");
    }
    { // 220/240: Faecher aus lkono Kindern 221 mit tex_id-Nachtrag und Startgeschwindigkeit als Versatz
        EffectSpawn spawn;spawn.id=220;spawn.type=2;spawn.lkono=4;spawn.scale={1.5f,3,2};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==4&&out.spawns[0].id==221&&out.spawns[0].tex_id_override==417&&out.spawns[0].flr_no==0&&out.spawns[0].has_offset&&out.spawns[3].mdlver==12&&close(out.spawns[0].scale.x,1.5f)&&s.type==0,"bhEff220 fan differs");
        const auto& e=out.spawns[1];NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,e.ax,e.ay,0);NJS_POINT3 v{0,0,-1},d;njCalcVector(&m,&v,&d);
        check(close(e.position.x,3*d.x)&&close(e.position.y,3*d.y)&&close(e.position.z,3*d.z)&&close(e.offset.x,2*d.x)&&close(e.offset.z,2*d.z),"bhEff220 child lies r along -z of its angles and carries spd as offset");
        EffectSprite child;initialize_effect_sprite(child,e);check(child.tex_id==417&&close(child.lox,2*d.x),"tex_id patch must reach the new work");
        s=work(240,1);s.lkono=1;step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].tex_id_override==421&&out.spawns[0].flr_no==2,"bhEff240 differs");
    }
}
static void group2_sprites(){
    EffectContext c;bool queued=false;take_effect_output();
    { // 198: Opak-Liste jeden Pass
        auto s=work(198,0);step_effect_sprite(s,c,&queued);
        check(queued&&s.opaque_queued&&s.flg==0x4100001u&&s.tex_id==94&&close(s.tv[3].u,0.625f)&&close(s.tv[3].v,0.46875f),"bhEff198 opaque frame differs");
        step_effect_sprite(s,c,&queued);check(queued&&s.opaque_queued,"bhEff198 stays in the opaque list");
    }
    { // 186: Eckpunkte aus Kamerawinkeln; ohne Kamera Einheitsmatrix
        EffectSpawn spawn;spawn.id=186;spawn.type=7;spawn.ay=8192;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,0,8192,0);NJS_POINT3 v{1,-1,0},d;njCalcVector(&m,&v,&d);
        check(queued&&s.tex_id==410&&s.type==0&&close(s.tv[1].x,d.x)&&close(s.tv[1].z,d.z)&&close(s.tv[0].v,0),"bhEff186 corners without camera differ");
        EffectContext cam=c;cam.camera_ready=true;cam.camera_angles={1000,20000,300};
        njUnitMatrix(&m);njRotateZ(&m,300);njRotateX(&m,1000);njRotateY(&m,20000);njRotateXYZ(&m,0,8192,0);njCalcVector(&m,&v,&d);
        step_effect_sprite(s,cam,&queued);check(queued&&close(s.tv[1].x,d.x)&&close(s.tv[1].y,d.y)&&close(s.tv[1].z,d.z)&&s.ct0==2,"bhEff186 corners under cam.mtx differ");
        step_effect_sprite(s,cam,&queued);step_effect_sprite(s,cam,&queued);step_effect_sprite(s,cam,&queued);check(!queued&&s.flg==0,"bhEff186 ends after four frames");
    }
    { // 189: fliegt bis unter lkono, dann Ruhe an der Ausgangslage
        EffectSpawn spawn;spawn.id=189;spawn.type=1;spawn.lkono=-3;spawn.scale={1,1,2};spawn.position={0,1,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==50&&s.ani_ct==0&&s.bl_dst==3&&(s.flg&0x4180000u)&&close(s.zn,0.2f)&&close(s.py,1),"bhEff189 init differs");
        unsigned passes=1;while(s.type!=0&&passes<200){step_effect_sprite(s,c,&queued);++passes;}
        check(s.type==0&&close(s.py,1)&&s.mode0==0&&close(s.sz,2),"bhEff189 must rest at its origin below lkono");
    }
    { // 191/193/195: Zeilenkaesten mit Groesse aus w/h; 195 ueber sz Passes gestreckt
        recvx_core_random_seed(2);auto s=work(191,0,{2,2,5});step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==429&&s.bl_dst==3&&close(s.sx,4*2*(40/256.0f))&&close(s.tv[0].u,0)&&close(s.tv[1].u,39/256.0f)&&close(s.sxb,2),"bhEff191 first row differs");
        float speed=std::sqrt(s.xn*s.xn+s.yn*s.yn+s.zn*s.zn);check(close(speed,0.5f),"bhEff191 start speed is sz/10");
        for(unsigned pass=1;pass<20;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff191 row");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff191 ends after 20 rows");
        s=work(195,2,{1,1,8});step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==431&&s.ani_ct==2&&s.ct1==0&&close(s.sx,4*(24/256.0f)),"bhEff195 first row differs");
        for(unsigned pass=1;pass<8;++pass){step_effect_sprite(s,c,&queued);check(queued&&s.ct1==int(float(pass*16)/8.0f),"bhEff195 stretch differs");}
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff195 ends after sz passes");
        s=work(195,3);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff195 type 3 is outside its tables");
        recvx_core_random_seed(4);float r[4];for(auto& x:r)x=frac();recvx_core_random_seed(4);
        s=work(193,0,{2.5f,3.25f,4.5f});step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==430&&s.bl_dst==10&&close(s.aox,0.5f)&&close(s.aoy,0.25f)&&close(s.spd,3)&&close(s.aoz,0.5f)&&close(s.sx,0.1875f*2)&&s.ct0==int(0.25f*(3*r[3]))&&s.ct1==0,"bhEff193 split of the scale fractions differs");
    }
    { // 196/205/210..213/233/235/244: Schleifen- und Anzeigebilder
        auto s=work(196,8);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==414&&(s.flg&0x4080000u)&&!(s.flg&0x100000u)&&s.ani_ct==1&&close(s.tv[0].v,64/256.0f)&&close(s.tv[1].u,31/256.0f),"bhEff196 colour/kind differ");
        for(unsigned pass=0;pass<8;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.ct0==0,"bhEff196 loops its eight frames");
        s=work(205,1,{1,1,2});step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==61&&s.ct1==1&&s.ct0==0&&close(s.tv[1].u,79/256.0f)&&close(s.tv[2].v,31/256.0f),"bhEff205 first frame");
        step_effect_sprite(s,c,&queued);check(s.ct0==1&&s.ct1==0,"bhEff205 advances after int(sz) passes");
        for(unsigned pass=0;pass<26;++pass)step_effect_sprite(s,c,&queued);check(s.mode0==2&&s.ct0==0,"bhEff205 enters the two-frame tail");
        step_effect_sprite(s,c,&queued);check(queued&&close(s.tv[0].u,160/256.0f)&&close(s.tv[0].v,128/256.0f)&&s.ct0==1,"bhEff205 tail frame");
        s.type=0;s.mode1=0;step_effect_sprite(s,c,&queued);check(!queued&&s.mode0==0&&(s.flg&0x1000000u),"bhEff205 idle resets");
        s=work(210,1,{1,1,90});step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==65&&s.ani_ct==0&&s.az==short(int((65536.0f*90)/360.0f))&&close(s.tv[2].v,47/256.0f),"bhEff210 first frame");
        for(unsigned pass=1;pass<20;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff210 frame");}step_effect_sprite(s,c,&queued);check(!queued&&s.type==0&&s.mode0==0&&s.flg,"bhEff210 rests after 20 frames");
        s=work(212,1);for(unsigned pass=0;pass<17;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.ani_ct==2&&s.ct0==0&&s.tv[0].col==0x80ffffffu,"bhEff212 loops with alpha 0x80");
        s=work(213,5);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==67&&close(s.tv[0].v,60/256.0f)&&close(s.tv[1].u,79/256.0f)&&close(s.tv[2].v,119/256.0f),"bhEff213 row (type-1)%3");
        EffectSpawn spawn;spawn.id=233;spawn.type=1;spawn.lkono=4;spawn.scale={1,1,2};spawn.ax=777;spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==416&&s.tv[0].x==0&&s.tv[1].x==2&&s.az==777&&close(s.tv[1].u,223/256.0f)&&close(s.tv[2].v,32/256.0f),"bhEff233 width/angle differ");
        for(unsigned pass=1;pass<8;++pass)step_effect_sprite(s,c,&queued);check(queued&&close(s.tv[0].v,96/256.0f),"bhEff233 fourth stripe");
        step_effect_sprite(s,c,&queued);check(!queued&&s.type==0&&s.mode0==0,"bhEff233 rests after 4*sz passes");
        s=work(235,1+1+2*3+12*2,{1,1,3});step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==418&&s.tv[0].col==0xff0000ffu&&close(s.tv[0].u,0.703125f)&&close(s.tv[2].v,120/256.0f)&&close(s.tv[0].v,159/256.0f),"bhEff235 fixed symbol differs");
        s=work(235,1,{1,1,3});step_effect_sprite(s,c,&queued);check(s.ct1==0&&s.ct0==1&&close(s.tv[1].u,59/256.0f),"bhEff235 column 0");step_effect_sprite(s,c,&queued);check(s.ct1==1&&close(s.tv[0].u,60/256.0f),"bhEff235 column 1");
        s=work(244,6,{1,1,2});step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==424&&s.ani_ct==1&&s.ct0==1&&s.ct1==1&&close(s.tv[0].u,32/256.0f)&&close(s.tv[1].u,32/256.0f+0.125f),"bhEff244 kind/colour differ");
    }
    { // 207/209: Wurfbahnen; 209 Kasten bis u+w
        recvx_core_random_seed(5);auto s=work(207,3,{1,1,16});step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==62&&(s.flg&0x4080000u)&&!(s.flg&0x100000u)&&s.ax==0&&close(s.tv[0].v,47/256.0f)&&close(s.tv[2].v,0),"bhEff207 flat/mirrored kind differs");
        check(close(std::sqrt(s.xn*s.xn+s.yn*s.yn+s.zn*s.zn),1.6f),"bhEff207 start speed sz/10");
        step_effect_sprite(s,c,&queued);check(close(s.tv[0].u,48/256.0f),"bhEff207 row 1");
        for(unsigned pass=2;pass<24;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff207 row");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff207 ends after 24 rows");
        EffectSpawn spawn;spawn.id=209;spawn.scale={1,1,180};spawn.ay=16384;spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==63&&s.az==short(int((65536.0f*180)/360.0f))&&close(s.yn,-1.6f)&&close(s.tv[1].u,16/256.0f)&&close(s.tv[3].v,16/256.0f),"bhEff209 init differs");
        step_effect_sprite(s,c,&queued);check(close(s.py,-1.6f)&&close(s.yn,(-1.6f-0.0326f)*0.95f),"bhEff209 fall step");
    }
    { // 216/223/225/232/237/239/242/249: Spritzer, Funken, Einschlaege
        EffectSpawn spawn;spawn.id=216;spawn.type=4;spawn.scale={2,2,4};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==66&&s.ani_ct==1&&s.tv[0].col==0xff404040u&&s.tv[0].y==-2&&close(s.yn,1-0.1f)&&close(s.sx,(2*7)/16.0f)&&close(s.tv[1].u,7/256.0f)&&s.ct0==1,"bhEff216 first row differs");
        for(unsigned row=1;row<14;++row)step_effect_sprite(s,c,&queued);check(queued,"bhEff216 row 13");step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff216 ends at u<0");
        recvx_core_random_seed(11);float r[8];for(auto& x:r)x=frac();recvx_core_random_seed(11);
        spawn=EffectSpawn{};spawn.id=223;spawn.type=3;spawn.scale={1,1,0.5f};spawn.ax=4000;spawn.ay=16380;spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==411&&s.ani_ct==1&&s.ct1==15&&s.ct2==int(18*r[0])&&std::abs(s.ct3)==int(2*r[1])+1&&s.az==int(4000*r[5]-2000)&&s.ax==int(65536*r[6])&&close(s.spd,0.5f*35.533333f*(1+r[7])),"bhEff223 init randomisation differs");
        check(s.tv[0].col==0xffffffffu&&close(s.tv[0].u,float(eff223_u0(s.ct2)+2)/256.0f),"bhEff223 first frame");
        step_effect_sprite(s,c,&queued);check(s.ct0==1&&s.tv[0].col==((unsigned(0xff-(0xff/15))<<24)|0xffffffu),"bhEff223 alpha fades with ct0/ct1");
        s=work(225,2,{2,2,1});step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==428&&s.bl_dst==6&&close(s.sx,4*2*(8/256.0f))&&close(s.tv[0].u,84/256.0f),"bhEff225 row 0");
        for(unsigned row=1;row<13;++row){step_effect_sprite(s,c,&queued);check(queued,"bhEff225 row");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff225 ends after its rows");
        s=work(232,3);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==415&&s.ani_ct==1&&close(s.tv[0].v,80/256.0f),"bhEff232 row");
        spawn=EffectSpawn{};spawn.id=237;spawn.type=1;spawn.scale={1,1,10};spawn.position={1,2,3};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==420&&close(s.lox,1)&&close(s.pz,4)&&close(s.yn,-0.0489f)&&s.ct0==1,"bhEff237 throw");
        for(unsigned pass=1;pass<15;++pass)step_effect_sprite(s,c,&queued);check(queued,"bhEff237 row 15");step_effect_sprite(s,c,&queued);check(!queued&&close(s.px,1)&&close(s.pz,3)&&s.type==0,"bhEff237 returns home");
        spawn=EffectSpawn{};spawn.id=239;spawn.mdlver=3;spawn.scale={2,8,1};spawn.has_velocity=true;spawn.velocity={1,0,0};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==2&&s.ani_ct==1&&s.ct0==4&&close(s.sx,0.8f*2*0.125f)&&close(s.px,1)&&close(s.xn,0.97f)&&s.tv[0].col==((unsigned(128-(128*4)/15)<<24)+0xffffffu)&&close(s.tv[1].u,32/256.0f+0.125f),"bhEff239 first frame differs");
        for(unsigned pass=5;pass<15;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff239 row");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff239 ends at row 15");
        recvx_core_random_seed(13);spawn=EffectSpawn{};spawn.id=242;spawn.type=1;spawn.scale={8,2.5f,3.2f};spawn.position={0,10,0};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==422&&s.ani_ct==1&&(s.flg&0x4080000u)&&close(s.sz,1)&&s.aox>=2048&&s.aox<10240,"bhEff242 init differs");
        check(close(s.sx,(s.sxb*8)/4)&&(s.sxb==0.03125f||s.sxb==0.0625f||s.sxb==0.09375f),"bhEff242 size class");
        unsigned passes=1;while(s.flg&&passes<2000){step_effect_sprite(s,c,&queued);++passes;}check(!s.flg&&passes<2000,"bhEff242 must end below start-20");
        spawn=EffectSpawn{};spawn.id=249;spawn.type=1;spawn.lkono=4;spawn.scale={2.5f,3.5f,10};spawn.position={1,1,1};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==427&&close(s.aox,0.05f)&&close(s.sx,0.4f)&&close(s.aoy,0.05f)&&close(s.sy,0.6f)&&close(s.zn,1)&&s.ct1==0,"bhEff249 init differs");
        step_effect_sprite(s,c,&queued);check(s.ct1==4&&close(s.pz,2)&&close(s.yn,-0.01f*0.95f)&&close(s.sx,0.4f*1.05f),"bhEff249 pass 2");
        step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&close(s.px,1)&&close(s.sx,2.5f)&&s.type==0,"bhEff249 returns home after lkono passes");
    }
    { // 203/227/248: Elternsatz
        EffectSprite parent;{EffectSpawn ps;ps.id=202;ps.flg=1;initialize_effect_sprite(parent,ps);}parent.ay=1000;parent.ax=500;parent.spd=0.5f;parent.sx=2;parent.sy=3;parent.px=5;parent.py=6;parent.pz=7;parent.type=1;
        auto s=work(203,1,{1,1,2});bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff203 without parent is a gap");
        s=work(203,1,{1,1,2});s.mdlver=2;s.parent=&parent;step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==60&&s.ct1==30&&close(s.tv[0].u,0)&&close(s.tv[0].v,128/256.0f)&&s.tv[0].col==0xffffffffu,"bhEff203 kind 1 init");
        step_effect_sprite(s,c,&queued);check(queued&&close(s.py-1.0f*2,s.py-2)&&s.tv[0].col==((unsigned(0xff-(0xff/30))<<24)|0xffffffu)&&close(s.sx,1-0.02f),"bhEff203 kind 1 step");
        parent.stflg|=0x1000000u;step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff203 dies with its parent");parent.stflg=0;
        s=work(227,1);s.mdlver=2;s.parent=&parent;step_effect_sprite(s,c,&queued);
        NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,1000);njRotateX(&m,500);njRotateY(&m,(2*0xffff)/8);NJS_POINT3 v{0,0,3},d;njCalcVector(&m,&v,&d);
        check(queued&&s.tex_id==413&&close(s.px,5+d.x)&&close(s.py,6+d.y)&&close(s.pz,7+d.z)&&close(s.sx,2)&&close(s.sz,1)&&close(s.tv[0].u,48/256.0f),"bhEff227 ring position differs");
        for(unsigned pass=0;pass<6;++pass)step_effect_sprite(s,c,&queued);check(queued&&s.ct0==0,"bhEff227 loops six frames");
        parent.mode1=1;step_effect_sprite(s,c,&queued);check(queued&&s.mode0==2&&close(s.tv[0].u,96/256.0f),"bhEff227 end row starts on parent mode1 1");
        for(unsigned pass=1;pass<13;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff227 end row");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff227 ends after 13 rows");parent.mode1=0;
        recvx_core_random_seed(9);s=work(248,0,{2.5f,20.3f,4.5f});s.parent=&parent;step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==426&&close(s.aoy,20)&&s.ct2==int(10.0f*(20.3f-20.0f))&&close(s.aoz,0.25f)&&close(s.sz,4)&&close(s.aox,0.1f)&&close(s.sx,2)&&(s.tv[0].col>>24)==unsigned(int((255.0f*20)/40.0f))&&((s.tv[0].col>>16)&0xff)==0xff,"bhEff248 parameters differ");
        parent.type=0;step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff248 ends when the parent rests");parent.type=1;
        // Verlinkter Elternsatz: die Drehung beginnt mit dessen Gelenkmatrix (njCalcVector).
        install_group4_services();
        parent.flg|=0x80u;parent.link_kind=1;parent.link_index=2;parent.lkono=3;
        recvx_core_random_seed(9);float rr[4];for(auto& x:rr)x=frac(); /* rr[0]: Streuung von zn (aoz 0) */
        recvx_core_random_seed(9);s=work(248,0,{2,2,2});s.ax=100;s.ay=200;s.parent=&parent;step_effect_sprite(s,c,&queued);
        {NJS_MATRIX r;njUnitMatrix(&r);njTranslate(&r,10.0f+3.0f,20.0f,30.0f);njRotateY(&r,8192);
         njRotateXYZ(&r,100,200,0);njRotateZ(&r,Angle(65536.0f*rr[1]));njRotateX(&r,Angle(512.0f*(rr[2]-rr[3])));
         NJS_POINT3 v0{0,0,2.0f/10.0f},e;njCalcVector(&r,&v0,&e);
         check(s.flg&&close(s.xn,e.x)&&close(s.yn,e.y)&&close(s.zn,e.z),"bhEff248 under a linked parent must start in the parent's joint frame");}
        parent.link_index=9;s=work(248,0,{2,2,2});s.parent=&parent;take_effect_output();step_effect_sprite(s,c,&queued);
        check(s.flg==0&&take_effect_output().gaps.size()==1,"bhEff248 under a linked parent without its node reports the gap");
        parent.flg&=~0x80u;parent.link_kind=-1;set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});
    }
    { // 246: Art 0 faellt bis lkono und spritzt; Art 1 Rolle/Kipp mit Wachstum aus den Nachkommaanteilen
        EffectSpawn spawn;spawn.id=246;spawn.type=0;spawn.lkono=0;spawn.mdlver=1;spawn.scale={1,1,10};spawn.position={0,0.3f,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        recvx_core_random_seed(6);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==425&&s.ani_ct==1&&s.tv[0].x==-0.21875f&&close(s.zn,1)&&close(s.spd,0.01f)&&s.ct0<8,"bhEff246 kind 0 init");
        unsigned passes=1;while(s.mode0==1&&passes<400){step_effect_sprite(s,c,&queued);++passes;}
        check(s.mode0==2&&close(s.py,0)&&s.tv[0].col==0xc0ffffffu&&close(s.yn<=0?-s.yn:s.yn,s.yn<=0?-s.yn:s.yn),"bhEff246 kind 0 lands at lkono");
        for(unsigned pass=0;pass<24;++pass)step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff246 splash ends after 24 passes");
        spawn.type=1;spawn.scale={2.5f,3.5f,4.25f};initialize_effect_sprite(s,spawn);recvx_core_random_seed(6);step_effect_sprite(s,c,&queued);
        check(queued&&close(s.aox,0.1f)&&close(std::fabs(s.sx),0.4f*1.1f)&&close(s.aoy,0.1f)&&close(s.sy,0.6f*1.1f)&&s.ct0>=1&&s.ct0<=4,"bhEff246 kind 1 growth differs");
    }
    { // 147/148: Bodenplatte und Blutstropfen
        auto s=work(147,1);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==80&&(s.flg&0x4000001u)==0x4000001u&&s.tv[0].col==0xff5f0000u&&s.tv[1].z==1&&s.tv[0].y==0,"bhEff147 plate differs");
        s.stflg|=0x1000000u;step_effect_sprite(s,c,&queued);check(!queued,"bhEff147 hides with stflg 0x1000000");
        std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100}}};GroundCollision ground;ground.update(walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
        EffectSpawn spawn;spawn.id=148;spawn.scale={3,3,3};spawn.position={0,5,0};spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(!queued&&s.tex_id==4&&s.ani_ct==2&&close(s.sx,0.3f)&&(s.flg&0x4120000u)==0x4120000u&&close(s.ofy,5)&&s.tv[0].col==0xff400000u,"bhEff148 init/mode1 0 differs");
        s.mode1=1;unsigned passes=0;while(s.mode1==1&&passes<200){step_effect_sprite(s,c,&queued);check(queued,"bhEff148 draws while falling");++passes;}
        check(s.mode1==2&&close(s.py,s.gpy),"bhEff148 lands on the ground");step_effect_sprite(s,c,&queued);check(!queued&&s.mode1==0&&close(s.py,5)&&s.yn==0,"bhEff148 resets to ofy");
    }
    { // 018/124: Flamme und Tropfen mit Bodenabfrage
        std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100}}};GroundCollision ground;ground.update(walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
        EffectSpawn spawn;spawn.id=124;spawn.position={0,4,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(!queued&&s.tex_id==33&&s.bl_dst==10&&s.tv[0].y==-2&&close(s.ofy,4)&&s.mode0==1,"bhEff124 first pass only prepares");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff124 forming rows");}check(s.mode0==2&&s.ct0==5,"bhEff124 enters the fall");
        unsigned passes=0;while(s.mode0==2&&passes<200){step_effect_sprite(s,c,&queued);++passes;}check(s.mode0==3&&s.ct0==9&&close(s.py,s.gpy),"bhEff124 lands");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(queued,"bhEff124 splash rows 9..13");}step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff124 ends before row 14 (ct0 > 14 releases first)");
        spawn=EffectSpawn{};spawn.id=18;spawn.type=1;spawn.flg=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==3&&s.ani_ct==3&&s.tv[0].col==0xffe0e0e0u&&close(s.sx,4*0.125f),"bhEff018 kind 1 start row");
        for(unsigned row=1;row<7;++row)step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(queued&&s.tex_id==18&&s.ani_ct==0&&s.ct0==1&&close(s.tv[0].v,0),"bhEff018 kind 1 continues on fl");
        s.mode1=2;step_effect_sprite(s,c,&queued);check(s.type==4&&s.ct0==1&&close(s.tv[0].u,0.375f),"bhEff018 mode1 2 switches to fed");
        step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff018 fed ends");
        recvx_core_random_seed(8);const float r0=frac(),r1=frac();(void)r0;recvx_core_random_seed(8);
        spawn.type=10;spawn.mdlver=1;spawn.flr_no=2;spawn.scale={1,1,1};spawn.position={0,3,0};initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(queued&&s.ct1==0&&s.ct0==3&&close(s.gpy,ground.query({0,5,0}))&&close(s.py,3)&&close(s.yn,-(0.01f+0.1f*r1)),"bhEff018 kind 10 above ground drops its pause and starts falling");
        s.ct1=0;s.ct0=0;s.sxb=0.6f;step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==19&&out.spawns[0].type==3&&close(s.sxb,0.6f*0.9f),"bhEff018 kind 10 smokes at row 0 while above ground");
    }
}
// Gruppe 3: Erzeuger mit Raumdiensten (Wandspiegelung, Boden, Sprengschaden, bhSetEffect,
// Wetterzustand, Hintergrundstimme, Raumnummer).
static void group3_services(){
    EffectContext c;bool queued=false;take_effect_output();
    std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100}}};GroundCollision ground;ground.update(walls,100.f);
    set_effect_walls(&walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
    { // 165: 2*type Passes, jeder zweite ein 159 Art 1, Bodenabprall, Rueckkehr
        EffectSpawn spawn;spawn.id=165;spawn.scale={1,1,0.5f};spawn.position={0,0.2f,0};spawn.ax=4096;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);s.mode1=3;
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(!queued&&(s.flg&0x1000000u)&&s.type==3&&s.mode1==0&&s.ct0==5&&out.spawns.size()==1&&out.spawns[0].id==159&&out.spawns[0].type==1&&close(s.aoy,0.2f)&&s.axp==4096,"bhEff165 first pass differs");
        check(close(out.spawns[0].position.y,0.2f)&&close(out.spawns[0].position.z,0),"bhEff165 child is requested before the move");
        unsigned spawned=1;for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);spawned+=unsigned(take_effect_output().spawns.size());}
        check(spawned==3&&s.type==0&&s.ct0==0&&close(s.py,0.2f)&&close(s.pz,0)&&s.ax==4096,"bhEff165 must fire three times and return home");
        s.mode1=1;step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.size()==1&&s.ct0==1,"bhEff165 re-arms from mode1");
        set_effect_walls(nullptr,0);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}check(rejected,"bhEff165 without walls is a gap");set_effect_walls(&walls,100.f);
    }
    { // 170: wie 165 mit Wegsumme und Daempfung 0.1
        EffectSpawn spawn;spawn.id=170;spawn.scale={1,1,0.5f};spawn.position={0,5,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);s.mode1=1;
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==171&&close(s.loz,0.5f)&&close(s.spd,-0.1f)&&close(s.pz,0.5f),"bhEff170 first pass differs");
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.type==0&&close(s.pz,0)&&close(s.py,5),"bhEff170 returns home after 2 passes");
    }
    { // 016: nach lkono Passes Sprengschaden 20/150/100
        EffectSpawn spawn;spawn.id=16;spawn.lkono=2;spawn.position={1,2,3};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(take_effect_output().bombs.empty()&&s.flg,"bhEff016 waits");
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.bombs.size()==1&&close(out.bombs[0].radius,20)&&out.bombs[0].damage_max==150&&out.bombs[0].damage_min==100&&close(out.bombs[0].position.z,3)&&s.flg==0,"bhEff016 bomb differs");
    }
    { // 109: bhSetEffect 108 mit Link-Nummer; gm_flg 1 sperrt
        auto s=work(109,0);for(unsigned pass=0;pass<15;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff109 early");}
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==108&&out.spawns[0].flg==1&&out.spawns[0].lkono==13&&close(out.spawns[0].scale.x,1),"bhEff109 kind 0 fires every 16 passes with lkono 13");
        s=work(109,5);s.lkono=1;step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff109 kind 5 pass 1");step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].lkono==5&&s.ct0==0,"bhEff109 kind 5 fires after lkono+1 passes with lkono = type");
        EffectContext locked=c;locked.gm_flg=1;s=work(109,0);s.ct0=15;step_effect_sprite(s,locked,&queued);check(take_effect_output().spawns.empty()&&s.ct0==15,"bhEff109 is locked by gm_flg 1");
    }
    { // 115: Art 0 ohne Auftrag vier Spritzer 15 Art 0 (Groesse 0.5); mit Auftrag abklingend bis stflg; Art 1/2
        auto s=work(115,0);step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==4&&out.spawns[0].id==15&&out.spawns[0].type==0&&close(out.spawns[0].scale.x,0.5f)&&close(s.spd,1)&&s.ct0==4,"bhEff115 kind 0 idle differs");
        s.mode1=1;unsigned passes=0,spawned=0;while(!(s.stflg&0x1000000u)&&passes<400){step_effect_sprite(s,c,&queued);spawned+=unsigned(take_effect_output().spawns.size());++passes;}
        check(passes>=100&&passes<=101&&s.mode1==0&&spawned>0,"bhEff115 kind 0 must fade over ~100 passes (spd -= 0.01f in single precision) and then mark itself");
        s=work(115,1,{2,2,2});step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].type==3&&close(out.spawns[0].scale.x,2),"bhEff115 kind 1");
        s=work(115,2,{2,2,2});step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff115 kind 2 waits for mode1");s.mode1=1;step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==1&&out.spawns[0].id==124&&close(out.spawns[0].scale.x,0.2f)&&s.mode1==0,"bhEff115 kind 2 drop");
    }
    { // 113: Faecher 114, Raum 1-09 mit 196 statt 225
        auto s=work(113,1);step_effect_sprite(s,c,&queued);auto out=take_effect_output();check(out.spawns.size()==225&&out.spawns[0].id==114&&close(out.spawns[0].scale.x,1.5f)&&out.spawns[0].has_offset&&s.flg==0,"bhEff113 kind 1 differs");
        EffectContext room=c;room.stage_no=1;room.room_no=9;s=work(113,1);step_effect_sprite(s,room,&queued);check(take_effect_output().spawns.size()==196,"bhEff113 room 1-09 count");
        s=work(113,0);step_effect_sprite(s,c,&queued);out=take_effect_output();check(out.spawns.size()==64&&close(out.spawns[5].scale.x,0.4f)&&out.spawns[5].mdlver==20,"bhEff113 kind 0");
    }
    { // 102/105: Wetterzustand und Hintergrundstimmen
        auto& w=effect_weather();w=EffectWeather{};
        EffectSpawn spawn;spawn.id=102;spawn.type=1;spawn.scale={2,3,1};spawn.ay=100;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(!queued&&w.windr==100&&close(w.winds,2)&&w.windsb==0&&s.ct0==256,"bhEff102 first pass differs");
        step_effect_sprite(s,c,&queued);check(w.windrb==100&&close(w.windsb,2)&&w.windr==100+(int(182.04445f*(20.0f*njSin(256)))&0xffff)&&close(w.winds,2+3*njSin(256)),"bhEff102 swing differs");
        s=work(105,0);step_effect_sprite(s,c,&queued);check(take_effect_output().background_sounds.empty()&&s.mode0==0,"bhEff105 idle");
        s.mode1=1;step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.background_sounds.size()==1&&out.background_sounds[0].slot==1&&out.background_sounds[0].sound==770&&s.mode0==1&&s.ct0==16384-1536&&w.thunder==unsigned(std::fabs(255.0f*njSin(16384-1536))),"bhEff105 thunder start differs");
        unsigned passes=0;while(s.mode0==1&&passes<100){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(s.mode0==2&&w.thunder==0&&s.ct1==0,"bhEff105 decays into the afterglow");
        bool echo=false;for(unsigned pass=0;pass<200&&s.mode0==2;++pass){step_effect_sprite(s,c,&queued);for(const auto& b:take_effect_output().background_sounds)if(b.sound==771)echo=true;}
        check(echo&&s.mode0==0,"bhEff105 must play 771 at ct1 30 and rest after 180");
    }
}
// Gruppe 4: Prozeduren mit eigener Zeichenfunktion (sys->ef_fnc). Geprueft werden der
// Hilfsarbeitssatz (bhSetExtraEffectWork) samt Freigabe, die Zeichenausgabe als quads/lines
// und das Zeichentor pt_flg 0x10 (game.c:338/426), hinter dem auch der Zustandsvorschub der
// Original-Zeichenfunktionen liegt.
static int g_extra_next=0;static std::vector<int> g_extra_released;
static void install_group4_services(){
    g_extra_next=0;g_extra_released.clear();
    set_effect_extra_allocator([](){return g_extra_next++;},[](int slot){g_extra_released.push_back(slot);});
    set_effect_link_lookup([](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){
        if(kind>1||index>2||node>15)return false;
        njUnitMatrix(&out);njTranslate(&out,10.0f+float(node),20.0f,30.0f);njRotateY(&out,8192);return true;});
    set_effect_work_angle([](unsigned kind,unsigned index,int& ay){if(kind>1||index>2)return false;ay=kind?4096:16384;return true;});
}
static void group4_custom_draw(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;c.camera_position={100,0,200};c.camera_view={0,0,1};c.enemy_count=3;
    EffectContext dark=c;dark.pt_flg=0; // kein Zeichenpass
    bool queued=false;take_effect_output();
    std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100}}};GroundCollision ground;ground.update(walls,100.f);
    // rom->flrp fuer bhCheckFloorEffect: eine type-3-Flaeche mit prm0 0 deckt x < 100 ab.
    std::vector<CollisionWall> floors{{1,3,0,0,0,{-200,0,-200},{300,1,500},{0,0,0,0}}};
    set_effect_walls(&walls,100.f);set_effect_ground(&ground,&floors,nullptr);
    { // 100: 77 Tropfen um das Kameraauge, Linien erst im Zeichenpass, Wind treibt sie
        effect_weather()=EffectWeather{};
        auto s=work(100,0);step_effect_sprite(s,c,&queued);
        check(!queued&&(s.flg&0x1000000u)&&s.mode0==1&&s.extra&&s.extra_slot==0&&s.lines.empty(),"bhEff100 init differs");
        step_effect_sprite(s,dark,&queued);
        check(queued&&s.custom_queued&&s.lines.empty(),"bhEff100 without a draw pass enqueues but draws nothing");
        step_effect_sprite(s,c,&queued);
        check(queued&&s.lines.size()==77&&s.lines[0].colors[0]==0x10101010u&&s.lines[0].colors[1]==0x40303030u,"bhEff100 rain lines differ");
        {const auto a=s.lines[0].points[0],b=s.lines[0].points[1];
         check(close(std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)),14.0f),"bhEff100 line is 14 units long");}
        auto& w=effect_weather();w=EffectWeather{};w.winds=2.0f;w.windr=1024;
        const auto before=s.lines[0].points[0];step_effect_sprite(s,c,&queued);
        check(!close(s.lines[0].points[0].y,before.y),"bhEff100 wind must move the drops");
        s.stflg|=0x1000000u;step_effect_sprite(s,c,&queued);
        check(s.flg==0&&g_extra_released.size()==1&&g_extra_released[0]==0,"bhEff100 must release its extra work");
    }
    { // 107: Bodenabfrage entscheidet, fuenf Passes, Punktfilter auf Texturgruppe 32
        install_group4_services();
        auto s=work(107,0);step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&s.ct0==0&&s.extra,"bhEff107 init differs");
        step_effect_sprite(s,c,&queued);
        // Die Kamera steht bei x 100; die Flaeche mit prm0 0 reicht bis x 100, dahinter wird gezeichnet.
        check(queued&&s.custom_queued&&s.ct0==1&&!s.quads.empty(),"bhEff107 first draw differs");
        const auto& q=s.quads[0];
        check(q.category==32&&q.frame==0&&q.point_filter&&q.source==8&&q.destination==10&&!q.world_space,"bhEff107 quad differs");
        check(close(q.vertices[0].x,-0.7f)&&close(q.vertices[0].y,-1.4f)&&close(q.vertices[2].y,0)&&q.vertices[0].col==0x80808080u,"bhEff107 quad shape/colour differ");
        for(unsigned pass=1;pass<4;++pass)step_effect_sprite(s,c,&queued);
        check(s.ct0==4&&s.flg,"bhEff107 lives five passes");
        step_effect_sprite(s,c,&queued);check(s.flg==0&&!g_extra_released.empty(),"bhEff107 must release after five passes");
    }
    { // 022: Spur mit 132-Anforderungen in den ersten 16 Passes, Zeilen erst im Zeichenpass
        install_group4_services();
        EffectSpawn spawn;spawn.id=22;spawn.flr_no=1;spawn.has_offset=true;spawn.offset={1,0,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(s.mode0==1&&s.extra&&take_effect_output().spawns.empty(),"bhEff022 init differs");
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==132&&out.spawns[0].type==1&&out.spawns[0].has_offset&&close(s.px,1)&&close(s.yn,-0.01f),"bhEff022 first flight step differs");
        check(queued&&s.quads.size()==1&&s.quads[0].category==9&&s.quads[0].frame==1&&s.quads[0].source==8&&s.quads[0].destination==10,"bhEff022 trail quad differs");
        step_effect_sprite(s,dark,&queued);take_effect_output();
        check(queued&&s.quads.empty(),"bhEff022 without a draw pass lays no trail");
        unsigned passes=0;while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(s.flg==0&&passes==29&&!g_extra_released.empty(),"bhEff022 must release once ct0 passes 30");
    }
    { // 024: Wandabprall und 25-Anforderungen; zwei Spursaetze mit eigenen Blendpaaren
        install_group4_services();
        EffectSpawn spawn;spawn.id=24;spawn.has_offset=true;spawn.offset={0,0,1};spawn.position={0,5,0};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(s.mode0==1&&s.extra,"bhEff024 init differs");
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==25&&out.spawns[0].flg==1&&close(out.spawns[0].scale.z,0),"bhEff024 child request differs");
        check(queued&&s.quads.size()==1&&s.quads[0].category==19&&s.quads[0].source==8&&s.quads[0].destination==10,"bhEff024 near trail differs");
        // Der ferne Satz zeichnet erst ab seiner sechsten Zeile mit Blendpaar 11/3.
        for(unsigned pass=0;pass<6;++pass){step_effect_sprite(s,c,&queued);take_effect_output();}
        bool far_row=false;for(const auto& q:s.quads)if(q.source==11&&q.destination==3)far_row=true;
        check(far_row,"bhEff024 far trail must appear with blend 11/3");
        EffectContext locked=c;locked.cb_flg=0x40000000u;step_effect_sprite(s,locked,&queued);take_effect_output();
        check(s.flg,"bhEff024 keeps running with the wall check locked");
        unsigned passes=0;while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(s.flg==0&&!g_extra_released.empty(),"bhEff024 must release once ct0 passes 47");
    }
    { // 025/027: Funken aus dem Hilfsarbeitssatz
        install_group4_services();
        auto s=work(25,0);step_effect_sprite(s,c,&queued);check(s.mode0==1&&s.extra,"bhEff025 init differs");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(!queued,"bhEff025 draws only from pass 6");}
        step_effect_sprite(s,c,&queued);
        check(queued&&s.quads.size()==8&&s.quads[0].category==18&&s.quads[0].point_filter&&s.quads[0].source==8&&s.quads[0].destination==10,"bhEff025 spark quads differ");
        unsigned passes=0;while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);++passes;}
        check(s.flg==0&&!g_extra_released.empty(),"bhEff025 must release once ct0 passes 14");
        install_group4_services();
        s=work(27,0);step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        NJS_MATRIX hand;njUnitMatrix(&hand);njTranslate(&hand,19.0f,20.0f,30.0f);njRotateY(&hand,8192);
        NJS_POINT3 muzzle{WpnTab[20].wp_fps2.x,WpnTab[20].wp_fps2.y,WpnTab[20].wp_fps2.z},tip;njCalcPoint(&hand,&muzzle,&tip);
        check(close(s.px,tip.x)&&close(s.py,tip.y)&&close(s.pz,tip.z)&&s.ct0==1&&out.spawns.empty(),"bhEff027 must start at the weapon tip (joint 9)");
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"bhEff027 smoke too early");}
        step_effect_sprite(s,c,&queued);out=take_effect_output();
        check(out.spawns.size()==1&&out.spawns[0].id==2&&out.spawns[0].type==9&&out.spawns[0].flr_no==3&&close(out.spawns[0].scale.z,0.25f),"bhEff027 smoke request differs");
        check(queued&&s.quads.size()==16,"bhEff027 draws sixteen sparks");
        unsigned tail=0;while(s.flg&&tail<200){step_effect_sprite(s,c,&queued);take_effect_output();++tail;}
        check(s.flg==0&&tail==2&&!g_extra_released.empty(),"bhEff027 must release once ct0 passes 8");
    }
    { // 217/143: Partikelsaetze ueber P_WRK im Hilfsarbeitssatz
        install_group4_services();
        auto s=work(217,1,{2,90,3});step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&s.extra&&(s.flg&0x20000000u)&&s.ct0>=10&&s.ct0<=20,"bhEff217 init differs");
        step_effect_sprite(s,c,&queued);
        // njPtclDrawPolygon(pos, 16, ..) gibt num = 16 Eckpunkte aus, zwei je Punkt: acht GS-Sprites
        // (ELF njPtclDrawPolygon 0x002D8334/0x002D845C, Ps2AddPrim3D-Schleife 0x002CBEBC).
        check(queued&&s.quads.size()==8&&!s.quads[0].textured&&s.quads[0].vertices[0].col==0xff322320u,"bhEff217 polygon particles differ");
        { // njPtclDrawPolygon: +-h Ninja-Bildschirmpunkte, unabhaengig von der Tiefe (ps2_NinjaPtcl.c:52-58)
          SceneCamera cam;cam.perspective=10922;NJS_MATRIX view;camera_view_matrix(cam,view);NJS_MATRIX inv;std::copy_n(&view[0],16,&inv[0]);njInvertMatrix(&inv);
          const float dist=320.0f/std::tan(0.0000958738f*float(cam.perspective/2));
          for(float depth:{20.0f,150.0f}){EffectSprite one=s;one.quads.resize(1);NJS_POINT3 vp{0,0,depth},wp;njCalcPoint(&inv,&vp,&wp);one.quads[0].center=wp;
            Scene sc;append_custom_effect(sc,one,cam,{},nullptr);check(sc.triangles.size()==2,"particle polygon not drawn");
            float xmin=1e9f,xmax=-1e9f;for(const auto& t:sc.triangles)for(const auto& v:t.vertices){NJS_POINT3 p=v.position,o;njCalcPoint(&view,&p,&o);const float x=o.x*1.174f*dist/o.z;xmin=std::min(xmin,x);xmax=std::max(xmax,x);}
            check(std::fabs((xmax-xmin)-2.0f*one.quads[0].vertices[1].x)<0.01f,"njPtclDrawPolygon size must be Ninja screen points at every depth");}}
        unsigned passes=1;while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);++passes;}
        check(!s.flg&&!g_extra_released.empty(),"bhEff217 must release at the end of its life");
        install_group4_services();
        s=work(143,0);step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&s.extra&&s.bl_dst==10&&(s.flg&0x20000001u)==0x20000001u,"bhEff143 init differs");
        step_effect_sprite(s,c,&queued);check(!queued&&s.mode0==2&&s.ct2==0xff&&s.ct0>=30&&s.ct0<=50,"bhEff143 second pass seeds the particles");
        step_effect_sprite(s,c,&queued);check(!queued,"bhEff143 stays idle while mode1 is 0");
        s.mode1=1;step_effect_sprite(s,c,&queued);
        // njPtclDrawSprite(pos, 16, 0.1, 0.15): acht GS-Sprites aus den Punkten 0..7; die halbe
        // Breite iz*w*dist traegt kein AspectW, in Sichtkoordinaten also 0.1/1.174 (ps2_NinjaPtcl.c:129/134).
        check(queued&&s.quads.size()==8&&s.quads[0].textured&&s.quads[0].category==4&&s.quads[0].frame<4&&s.quads[0].point_filter,"bhEff143 sprite particles differ");
        check(std::fabs(s.quads[0].vertices[1].x-0.1f/1.174f)<1e-6f&&std::fabs(s.quads[0].vertices[3].y-0.15f)<1e-6f,"bhEff143 GS sprite size differs from iz*w*dist without AspectW");
        check((s.quads[0].vertices[0].col>>24)==0xffu,"bhEff143 alpha comes from ct2");
    }
    { // 243: Rollwinkel aus der Kamera, vier bzw. zwoelf Bilder
        install_group4_services();
        EffectContext cam=c;cam.camera_ready=true;cam.camera_angles={0,16384,0};
        auto s=work(243,1);s.lkono=3;step_effect_sprite(s,cam,&queued);
        check(queued&&s.custom_queued&&s.tex_id==423&&s.ct2==1&&s.tv[0].col==0xffff0000u&&!s.quads.empty(),"bhEff243 first frame differs");
        check(s.quads[0].world_space&&s.quads[0].category==423&&s.quads[0].source==8&&s.quads[0].destination==3,"bhEff243 quad differs");
        for(unsigned pass=0;pass<3;++pass)step_effect_sprite(s,cam,&queued);
        check(s.ct2==0,"bhEff243 even kinds loop four frames");
        s=work(243,2);for(unsigned pass=0;pass<11;++pass)step_effect_sprite(s,cam,&queued);
        check(s.ct1==11&&s.flg,"bhEff243 odd kind runs twelve frames");
        step_effect_sprite(s,cam,&queued);check(s.type==0&&s.mode0==0,"bhEff243 odd kind rests after twelve frames");
    }
    { // 236: Band zwischen eigener Lage und dem Zielgelenk
        install_group4_services();
        EffectSpawn spawn;spawn.id=236;spawn.type=1;spawn.lkono=4;spawn.scale={1,2,3};spawn.position={0,0,0};spawn.ax=1;spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==419&&s.ct0==1&&s.ct1==0&&!s.quads.empty()&&s.quads[0].world_space,"bhEff236 first pass differs");
        check(close(s.tv[0].x,-3.0f)&&close(s.tv[1].x,3.0f)&&close(s.tv[0].y,0),"bhEff236 upper edge spans sz around its own position");
        check(close(s.tv[2].y,22.0f)&&close(s.tv[3].y,22.0f),"bhEff236 lower edge sits sy above the target joint");
        for(unsigned pass=0;pass<4;++pass)step_effect_sprite(s,c,&queued);
        check(s.ct1==7&&s.ct0==5,"bhEff236 stretches its row over lkono passes");
        spawn.ax=-1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);check(queued,"bhEff236 ax < 0 uses the player");
    }
    { // 149: wartet auf mode1, waechst, erzeugt 264, blendet aus
        install_group4_services();
        auto s=work(149,1,{2,2,1});step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==6&&s.tex_id==80&&s.ani_ct==1&&s.tv[0].y==-2&&s.tv[2].y==0&&s.az==int(182.04445f*90)&&close(s.ah,0.2f)&&close(s.aw,1.0f),"bhEff149 init differs");
        step_effect_sprite(s,c,&queued);check(!queued&&s.mode0==6,"bhEff149 waits for its script order");
        s.mode1=1;step_effect_sprite(s,c,&queued);check(s.mode0==1,"bhEff149 starts on mode1 1");
        unsigned passes=0;while(s.mode0==1&&passes<40){step_effect_sprite(s,c,&queued);++passes;}
        check(s.mode0==2&&close(s.ah,1.0f)&&queued&&!s.quads.empty()&&!s.quads[0].fogged&&s.quads[0].category==80&&s.quads[0].frame==1,"bhEff149 grown state/draw differ");
        unsigned spawned=0;for(unsigned pass=0;pass<700&&s.mode0==2;++pass){step_effect_sprite(s,c,&queued);for(const auto& e:take_effect_output().spawns)if(e.id==264)++spawned;}
        check(spawned>=1,"bhEff149 must request its 264 column");
        s.mode1=2;while(s.mode0==2&&passes<800){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(s.mode0==3,"bhEff149 mode1 2 starts the fade");
    }
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});
}
// Gruppe 5: Explosionsball, Rauchreihen, texturloses Dreieck, Ringfuehrung, Abdunklung.
static void group5_rest(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    { // 012: texturloses Dreieck (pn 3) mit eigener Farbe je Ecke, Flug gegen ay
        EffectSpawn spawn;spawn.id=12;spawn.type=0;spawn.ay=16384;spawn.scale={1,1,1};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        recvx_core_random_seed(17);float r[2];for(auto& x:r)x=frac();recvx_core_random_seed(17);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.flg==0x4100001u&&s.pn==3&&s.untextured&&s.gouraud&&s.tv[0].col==0xff800000u&&s.tv[2].col==0xff200000u,"bhEff012 init differs");
        check(close(s.xn,(0.5f*(0.2f+r[0]))-0.05f*(0.5f*(0.2f+r[0])))&&close(s.yn,0.5f*r[1]-0.04f)&&s.ct0==1,"bhEff012 first step differs");
        unsigned passes=1;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}
        check(!s.flg&&passes==22,"bhEff012 kind 0 must end after 22 passes");
        spawn.type=1;initialize_effect_sprite(s,spawn);passes=0;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);++passes;}
        check(!s.flg&&passes==14,"bhEff012 kind 1 must end after 14 passes");
        spawn.type=2;initialize_effect_sprite(s,spawn);bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"bhEff012 accepts a kind outside its original speed cases");
    }
    { // 019: vier Reihen; Art 0 setzt rom->lgtp[0] (ohne Lichtbesitzer benannte Luecke)
        auto s=work(19,0);step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(queued&&s.tex_id==19&&s.bl_dst==10&&close(s.py,1.0f)&&close(s.yn,0.9f),"bhEff019 kind 0 rise differs");
        check(out.gaps.size()==1&&out.gaps.begin()->find("rom->lgtp")!=std::string::npos,"bhEff019 must report the unbound room light");
        // Mit Lichtbesitzer: effsub1.c bhEff019 type 0 schreibt lgtp[0] nur ohne flg 0x2, rgb 3/2/1, nr 20, fr 60, Lage des Satzes.
        bool lit=false,cond=false;NJS_POINT3 at{},rgb{};float nr=0,fr=0;
        EffectWorldHooks h;h.room_light0=[&](NJS_POINT3 p,NJS_POINT3 c3,float n,float f,bool only){lit=true;at=p;rgb=c3;nr=n;fr=f;cond=only;};set_effect_world_hooks(h);
        auto lamp=work(19,0);lamp.px=4;lamp.pz=-2;step_effect_sprite(lamp,c,&queued);out=take_effect_output();
        check(lit&&cond&&close(at.x,4)&&close(at.z,-2)&&close(rgb.x,3)&&close(rgb.y,2)&&close(rgb.z,1)&&close(nr,20)&&close(fr,60)&&out.gaps.empty(),"bhEff019 kind 0 must set rom->lgtp[0] only while its flg 0x2 is clear");
        set_effect_world_hooks({});
        s=work(19,1);step_effect_sprite(s,c,&queued);
        check(queued&&s.bl_src==11&&s.bl_dst==3&&s.ct1==72&&s.tv[0].col==((72u<<24)|0xffffffu)&&close(s.py,1.4f),"bhEff019 kind 1 fade-in differs");
        s=work(19,2);step_effect_sprite(s,c,&queued);
        check(queued&&s.bl_dst==6&&close(s.sxb,1.5f)&&close(s.sx,8.0f*1.5f*0.09375f),"bhEff019 kind 2 growth differs");
        for(unsigned pass=1;pass<7;++pass)step_effect_sprite(s,c,&queued);
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"bhEff019 kind 2 ends after seven rows");
    }
    { // 023: Feuerball mit Teilung; die Funken erben die gestreute Lage der Vorlage
        EffectSpawn spawn;spawn.id=23;spawn.type=1;spawn.mdlver=0;spawn.flr_no=0;spawn.scale={2,2,2};spawn.position={1,2,3};spawn.flg=1;EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.flg==0x100001u&&s.tex_id==9&&s.bl_src==8&&s.bl_dst==6&&close(s.spd,1.2f)&&s.ct0==1&&take_effect_output().spawns.empty(),"bhEff023 first frame differs");
        for(unsigned pass=0;pass<3;++pass){step_effect_sprite(s,c,&queued);take_effect_output();}
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==2&&out.spawns[0].id==23&&out.spawns[0].type==0&&close(out.spawns[0].scale.x,2.0f*1.2f)&&out.spawns[1].id==24&&out.spawns[1].mdlver==1&&out.spawns[1].has_offset,"bhEff023 split requests differ");
        check(close(out.spawns[1].position.x,out.spawns[0].position.x)&&close(out.spawns[1].position.z,out.spawns[0].position.z),"bhEff023 spark must inherit the jittered template position");
        check(close(s.spd,1.4f),"bhEff023 split grows the next ball");
        unsigned passes=0;while(s.flg&&passes<100){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(!s.flg,"bhEff023 must end at its UV terminator");
        spawn.mdlver=1;spawn.flr_no=0;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);take_effect_output();
        check(s.tex_id==10&&s.bl_dst==10,"bhEff023 mdlver 1 with flr_no 0 uses texture group 10");
        spawn.flr_no=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);take_effect_output();
        check(s.tex_id==9&&s.ani_ct==1,"bhEff023 mdlver 1 with flr_no 1 uses texture group 9");
    }
    { // 204: schwarze Vollkachel in der Opak-Liste
        auto s=work(204,0);step_effect_sprite(s,c,&queued);
        check(!queued&&!s.opaque_queued&&(s.flg&0x1000000u),"bhEff204 idle must stay invisible");
        s.mode1=1;step_effect_sprite(s,c,&queued);
        check(queued&&s.opaque_queued&&s.tex_id==449&&s.tv[0].col==0xff000000u&&close(s.tv[1].u,0.99609375f),"bhEff204 blackout differs");
    }
    { // 118: acht Kinder 117, deren Plaetze der Raumbesitzer in pv zurueckschreibt
        auto s=work(118,0,{1,1,1});s.slot=5;s.sy=2.0f;
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(out.spawns.size()==8&&s.ct3==8&&s.mode0==1&&s.ct2==1,"bhEff118 ring request differs");
        for(unsigned i=0;i<8;++i){
            check(out.spawns[i].id==117&&out.spawns[i].has_offset&&out.spawns[i].slot_sink_work==5&&out.spawns[i].slot_sink_index==int(i),"bhEff118 must ask for its child slot back");
            const auto& o=out.spawns[i].offset;check(close(o.x*o.x+o.z*o.z,1.0f)&&o.y==0,"bhEff118 children start on the unit ring");
        }
        // Ohne zurueckgeschriebene Plaetze (pv bleibt -1) laeuft die Fuehrung leer weiter.
        step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.ct2==2&&s.ct3==8,"bhEff118 keeps running without resolved children");
        s.mode1=1;s.ct3=0;step_effect_sprite(s,c,&queued);
        check((s.stflg&0x1000000u)&&s.mode0==0&&s.mode1==0,"bhEff118 must end once its order is done and no child is left");
    }
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// Gruppe 6: die beiden Prozeduren mit H-Eckpunkten (njDrawTexture3DHEx). Geprueft werden
// die Bildwahl aus mdlver samt negativer Reste unter 15, die Flugbahn, die Grundfarbe als
// Eckfarbe und der Grauwert als Nebelfarbe genau dieses Zeichenaufrufs.
static void group6_h_vertices(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;c.camera_view={0,0,1};bool queued=false;take_effect_output();
    { // 114: Truemmerstueck des Faechers bhEff113
        recvx_core_random_seed(23);float r[2];for(auto& x:r)x=frac();recvx_core_random_seed(23);
        EffectSpawn spawn;spawn.id=114;spawn.mdlver=20;spawn.scale={1,1,1};spawn.position={0,5,0};spawn.has_offset=true;spawn.offset={2,1,3};spawn.flg=1;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.custom_queued&&s.tex_id==31&&(s.flg&0x6000000u)&&s.tv[0].col==0xc0ffffffu&&s.bl_src==8&&s.bl_dst==10,"bhEff114 init differs");
        // mdlver 20 > 14: 32er-Kachel, u aus (20-15)%8, v aus (20-15)/8*32+144
        check(close(s.tv[0].u,float((5%8)*32)/256.0f)&&close(s.tv[0].v,float(((5/8)*32)+144)/256.0f)&&close(s.sx,7.0f*0.125f),"bhEff114 mdlver 20 tile differs");
        check(s.axp==(int(182.04445f*(10.0f+10.0f*r[0]))&0xffff)&&s.ayp==(int(182.04445f*(10.0f+10.0f*r[1]))&0xffff),"bhEff114 spin rates differ");
        check(close(s.lox,2.0f-0.06f*2.0f)&&close(s.px,2.0f)&&close(s.yn,1.0f-0.05f)&&close(s.py,5.0f+1.0f),"bhEff114 first flight step differs");
        check(!s.quads.empty()&&s.quads[0].world_space&&s.quads[0].category==31&&s.quads[0].fog_color_set,"bhEff114 draw differs");
        const unsigned grey=s.quads[0].fog_color;
        check((grey&0xffu)==((grey>>8)&0xffu)&&(grey&0xffu)==((grey>>16)&0xffu),"bhEff114 haze fog colour must be grey");
        // Raum 9-11 nimmt die schwaechere Grundfarbe.
        EffectContext room=c;room.stage_no=9;room.room_no=11;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,room,&queued);check(s.tv[0].col==0x60ffffffu,"bhEff114 room 9-11 base colour differs");
        // mdlver unter 15 rechnet mit negativen Resten und ergibt negative UV.
        spawn.mdlver=0;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(close(s.tv[0].u,0)&&close(s.tv[0].v,float((-15/5)*48)/256.0f)&&s.tv[0].v<0&&close(s.sx,8.0f*0.1875f),"bhEff114 mdlver below 15 must keep the original negative rows");
        spawn.mdlver=40;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(close(s.tv[0].v,0.9375f)&&close(s.tv[0].u,float((40-29)*16)/256.0f)&&close(s.sx,6.0f*0.0625f),"bhEff114 mdlver above 28 tile differs");
        // Fall unter Hoehe -10 gibt frei.
        spawn.mdlver=20;spawn.position={0,-9.5f,0};spawn.offset={0,-1,0};initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);check(s.flg==0,"bhEff114 must release below height -10");
        // Zeitlupe ohne gebundenen Teiler ist eine Luecke, kein geratener Wert.
        spawn.position={0,5,0};spawn.offset={2,1,3};initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        EffectContext slow=c;slow.st_flg=0x2000000u;bool rejected=false;
        try{step_effect_sprite(s,slow,&queued);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"bhEff114 slow motion without sys->ef_slow must report the gap");
        slow.ef_slow=2;step_effect_sprite(s,slow,&queued);check(s.flg,"bhEff114 runs with a bound slow-motion divisor");
    }
    { // 221: Teilchen des Faechers bhEff220/240
        EffectSpawn spawn;spawn.id=221;spawn.mdlver=16;spawn.flr_no=0;spawn.scale={1,1,1};spawn.position={0,20,0};spawn.has_offset=true;spawn.offset={1,0,1};spawn.tex_id_override=417;spawn.flg=1;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        check(s.tex_id==417,"bhEff221 must take the texture group its producer patches in");
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tv[0].col==0xffffffffu&&close(s.spd,10.0f)&&(s.flg&0x6000000u),"bhEff221 init differs");
        check(!s.quads.empty()&&!s.quads[0].world_space&&s.quads[0].category==417&&s.quads[0].fog_color==0,"bhEff221 flr_no 0 draws a billboard without haze");
        // Bit 2 richtet das Quad aus, Bit 4 schaltet den Grauwert ein.
        spawn.flr_no=6;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(s.quads[0].world_space&&s.quads[0].fog_color_set&&(s.quads[0].fog_color&0xffu)!=0,"bhEff221 flr_no bits 2/4 differ");
        // Ende 10 unter der Starthoehe.
        unsigned passes=1;while(s.flg&&passes<400){step_effect_sprite(s,c,&queued);++passes;}
        check(!s.flg&&passes<400,"bhEff221 must end ten units below its start height");
        // Ohne Texturgruppe des Erzeugers ist die Zeichnung eine Luecke.
        spawn.tex_id_override=-1;spawn.flr_no=0;initialize_effect_sprite(s,spawn);
        bool rejected=false;try{step_effect_sprite(s,c,&queued);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"bhEff221 without its producer's texture group must report the gap");
    }
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// Rueckseitentor flg 0x20000 (effect.c:1374) mit den dafuer portierten
// Ninja-Funktionen njProjectScreen (ps2_NaMatrix.c:1572) und njGetPlaneNormal2
// (ps2_NaColi.c:314), dazu bhEff145/bhEff_Sub146 (effsub6.c:75/114/160).
static void group7_backface_and_eyes(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    { // njProjectScreen: NaViewScreenMatrix MAL m, nicht umgekehrt.
        njSetPerspective(8192);
        NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,0,0,40.0f);njRotateY(&m,4096);
        NJS_POINT3 p{3.0f,-2.0f,1.0f},view;njCalcPoint(&m,&p,&view);
        NJS_POINT2 screen;njProjectScreen(&m,&p,&screen);
        check(close(screen.x,fNaViwAspectW*view.x*(_nj_screen_.dist/view.z)+fNaViwOffsetX)
            &&close(screen.y,fNaViwAspectH*view.y*(_nj_screen_.dist/view.z)+fNaViwOffsetY),"njProjectScreen multiplies in the wrong order");
        // njSetPerspective: (w/2) / tan(0.0000958738 * (ang/2)).
        check(close(_nj_screen_.dist,(_nj_screen_.w/2.0f)/std::tan(0.0000958738f*float(8192/2))),"njSetPerspective differs");
    }
    { // njGetPlaneNormal2 nimmt (p1-p0) x (p2-p1); njGetPlaneNormal dasselbe aus p[0..2].
        NJS_POINT3 a{0,0,0},b{1,0,0},d{1,1,0};NJS_VECTOR v;njGetPlaneNormal2(&a,&b,&d,&v);
        check(close(v.x,0)&&close(v.y,0)&&close(v.z,1.0f),"njGetPlaneNormal2 edge order differs");
        // Mit (p2-p0) statt (p2-p1) waere z hier 1 und bei diesem Dreieck nicht zu
        // unterscheiden; deshalb ein Dreieck, das beide Reihenfolgen trennt.
        NJS_POINT3 e{2.0f,0,0},f{3.0f,1.0f,0};njGetPlaneNormal2(&a,&e,&f,&v);
        check(close(v.z,2.0f),"njGetPlaneNormal2 must use the p1->p2 edge");
        NJS_POINT3 three[3]={a,e,f};NJS_VECTOR w;njGetPlaneNormal(three,&w);
        check(close(w.x,v.x)&&close(w.y,v.y)&&close(w.z,v.z),"njGetPlaneNormal must match njGetPlaneNormal2");
    }
    { // Das Tor am Zeichenpfad: Kamera im Ursprung, Blick auf Welt -z.
        SceneCamera camera;camera.perspective=8192;
        auto quad=work(146,0);quad.tex_id=4;quad.ani_ct=0;quad.flg=1u|0x20000u;
        Scene scene;unsigned stflg=0;
        append_effect_sprite(scene,quad,{0,0,-40.0f},{1,1,1},camera,0,nullptr,&stflg);
        check(scene.triangles.empty()&&(stflg&0x10u),"world-space quad with its local +z toward the camera must be culled");
        // Um 180 Grad gedreht zeigt die lokale -z-Seite zur Kamera.
        quad.ay=32768;scene=Scene{};stflg=0x10u;
        append_effect_sprite(scene,quad,{0,0,-40.0f},{1,1,1},camera,0,nullptr,&stflg);
        check(scene.triangles.size()==2&&!(stflg&0x10u),"the reverse side must draw and clear stflg 0x10");
        // Ein Billboard (0x100000) kehrt die Reihenfolge mit njScale(1,-1,-1) um
        // und kommt deshalb immer durch das Tor (bhEff148 flg 0x4120000).
        auto board=work(148,0);board.tex_id=4;board.ani_ct=0;board.flg=1u|0x120000u;
        scene=Scene{};append_effect_sprite(scene,board,{0,0,-40.0f},{1,1,1},camera,0);
        check(scene.triangles.size()==2,"a billboard must pass the backface gate");
        // Ohne das Tor zeichnet derselbe Weltquad in beiden Lagen.
        quad.ay=0;quad.flg=1u;scene=Scene{};
        append_effect_sprite(scene,quad,{0,0,-40.0f},{1,1,1},camera,0);
        check(scene.triangles.size()==2,"without flg 0x20000 nothing is culled");
    }
    { // bhEff145: Fuehrungssatz der Augenlichter an der Gelenkmatrix.
        NJS_MATRIX head;njUnitMatrix(&head);njTranslate(&head,10.0f,20.0f,30.0f);njRotateY(&head,16384);
        auto s=work(145,2);s.flg=1;s.slot=7;s.link_matrix=head;
        step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&s.stflg==0&&(s.flg&0x8000u)&&s.tv[0].col==0xffff0000u,"bhEff145 init differs");
        check(take_effect_output().spawns.empty(),"bhEff145 must not request anything in mode0 0");
        step_effect_sprite(s,c,&queued);
        auto out=take_effect_output();
        check(out.spawns.size()==2,"bhEff145 must request both eyes every pass");
        const int ax=int(10430.381f*std::atan2(head[6],head[10]));
        const int ay=int(10430.381f*std::asin(std::clamp(-head[2],-1.0f,1.0f)));
        const int az=int(10430.381f*std::atan2(head[1],head[0]));
        const NJS_POINT3 eyes[2]={{0.28f,0.82f,-0.90f},{-0.28f,0.82f,-0.90f}};
        for(unsigned i=0;i<2;++i){
            const auto& e=out.spawns[i];
            NJS_POINT3 local=eyes[i],expect;njCalcPoint(&head,&local,&expect);
            check(e.id==146&&e.type==2&&e.flg==1&&close(e.scale.x,0.05f)&&close(e.scale.y,0.05f)&&close(e.scale.z,0.05f)&&e.mdlver==0,"bhEff146_Init template differs");
            check(close(e.position.x,expect.x)&&close(e.position.y,expect.y)&&close(e.position.z,expect.z),"bhEff146_Init eye offset differs");
            check(e.ax==ax&&e.ay==ay&&e.az==az,"bhEff146_Init angles differ");
            check(e.parent_slot==7&&e.mode1==0&&e.has_color&&e.color==0xffff0000u,"bhEff146_Init post-write differs");
        }
        // mode1 == 2 blendet die Farbe in Schritten von 0x10000000 aus.
        s.mode1=2;step_effect_sprite(s,c,&queued);
        check(s.tv[0].col==0xefff0000u&&take_effect_output().spawns.size()==2,"bhEff145 fade step differs");
        unsigned passes=0;while(s.tv[0].col&&passes<32){step_effect_sprite(s,c,&queued);++passes;take_effect_output();}
        check(s.tv[0].col==0&&passes==15,"bhEff145 must reach colour 0 after fifteen more passes");
        // Ohne aufgeloeste Gelenkmatrix ist das eine benannte Luecke, kein Ersatzwert.
        auto plain=work(145,0);plain.flg=1;plain.mode0=1;plain.link_matrix=nullptr;
        step_effect_sprite(plain,c,&queued);
        auto gap=take_effect_output();
        check(gap.spawns.empty()&&gap.gaps.size()==1,"bhEff145 without a joint matrix must report the gap");
    }
    { // bhEff_Sub146: ein Augenlicht, vier Bilder bei type 2.
        EffectSpawn spawn;spawn.id=146;spawn.type=2;spawn.flg=1;spawn.scale={0.05f,0.05f,0.05f};
        spawn.has_color=true;spawn.color=0xffff0000u;spawn.mode1=0;
        EffectSprite s;initialize_effect_sprite(s,spawn);
        check(s.tv[0].col==0xffff0000u&&s.tv[1].col==0,"bhEff146_Init writes only the first corner colour");
        step_effect_sprite(s,c,&queued);
        check(queued&&s.mode0==2&&s.mode2==1&&s.tex_id==4&&s.ani_ct==2&&s.bl_src==8&&s.bl_dst==10,"bhEff_Sub146 init differs");
        check(s.flg==(1u|0x40a0000u)&&(s.flg&0x20000u),"bhEff_Sub146 must carry the backface gate");
        check(s.ct2==0x40000000&&close(s.sx,0.05f)&&close(s.tv[3].u,1.0f)&&close(s.tv[1].u,1.0f)&&close(s.tv[1].v,0)&&close(s.tv[2].v,1.0f),"bhEff_Sub146 table differs");
        // 0x00FF0000 - 0x40000000 laeuft unter Null in ein hohes Alpha.
        check(s.tv[0].col==0xc0ff0000u&&s.tv[3].col==s.tv[0].col,"bhEff_Sub146 first fade step differs");
        // Alpha 0xC0 -> 0x80 -> 0x40 werden gezeichnet; der Pass, der auf 0x00
        // kommt, gibt den Platz frei und reiht sich nicht mehr ein.
        unsigned drawn=1;while(s.flg&&drawn<16){step_effect_sprite(s,c,&queued);if(queued)++drawn;}
        check(!s.flg&&drawn==3,"bhEff_Sub146 type 2 must draw three passes");
        // type 0 nimmt den kleinsten Schritt, type 1 den mittleren.
        spawn.type=0;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(s.ct2==0x10000000&&s.tv[0].col==0xf0ff0000u,"bhEff_Sub146 type 0 step differs");
        spawn.type=1;initialize_effect_sprite(s,spawn);step_effect_sprite(s,c,&queued);
        check(s.ct2==0x20000000&&s.tv[0].col==0xe0ff0000u,"bhEff_Sub146 type 1 step differs");
        // mode1 != 0: ein Bild zeichnen, dann den Platz freigeben; die Farbe des
        // Fuehrungssatzes bleibt unangetastet.
        spawn.type=2;spawn.mode1=2;spawn.color=0x80ff0000u;initialize_effect_sprite(s,spawn);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.flg==0&&s.tv[0].col==0x80ff0000u&&s.tv[3].col==0x80ff0000u,"bhEff_Sub146 mode1 2 differs");
    }
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// effsub6.c:75/114/160 am Originalraum: RM_1002 ist der einzige Raum, dessen
// Effektliste 145 fuehrt (Eintrag 11: flg 1, type 2, lkflg 2 auf Gegner 1,
// Gelenk 5). Geprueft werden Erzeugung, Lebensdauer und das Rueckseitentor.
static void eye_lights(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_1002.RDT",64*1024*1024);
    const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_1002");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    const auto live=[&](unsigned id){unsigned n=0;for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&(s->flg&1u)&&s->id==id)++n;}return n;};
    world.tick();
    check(live(145)==1&&live(146)==0,"RM_1002 must hold one bhEff145 and no eye after the first pass");
    world.tick();
    check(live(146)==2,"bhEff145 must place both eyes on its second pass");
    for(unsigned i=0;i<4;++i)world.tick();
    const unsigned eyes=live(146);
    // type 2 zieht 0x40000000 je Pass ab: drei gezeichnete Bilder, also drei
    // Jahrgaenge mit je zwei Augen.
    check(eyes==6,"the eye lights must settle at three passes times two");
    unsigned visible=0;for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&s->id==146&&world.effect_visible(i))++visible;}
    check(visible==eyes,"every eye light must reach the draw pass");
    // Das Rueckseitentor entscheidet je Kamera. Die Augenvierecke sind an ihrer
    // Flaechenfarbe (reines Rot aus 0x00FF0000) erkennbar.
    const auto eye_triangles=[&](const SceneCamera& camera){
        Scene draw;std::vector<BoneMatrix> nodes;world.append(draw,nodes,camera);
        return std::count_if(draw.triangles.begin(),draw.triangles.end(),[](const SceneTriangle& t){
            return (t.color&0x00ffffffu)==0x00ff0000u&&t.blend==SceneBlend::Ninja;});};
    std::size_t drawing=0,culling=0;
    for(const auto& camera:stage.cameras){
        const auto count=eye_triangles(camera);
        if(count==long(2*eyes))++drawing;else if(count==0)++culling;
        else check(false,"a camera drew only part of the eye quads");
    }
    check(drawing>0&&culling>0,"the backface gate must cull the eyes from at least one original camera");
}
// effsub1.c:8720 bhEff127 — die Gaswand. Erzeugt nur Rauch (128), haelt
// sys->gas_py und toetet den Spieler, wenn die Wand ueber seinen Kopf steigt.
static void group8_gas_wall(){
    install_group4_services();
    std::vector<CollisionWall> walls{{1,0,0,0,0,{-200,-10,-200},{400,10,400}}};GroundCollision ground;ground.update(walls,100.f);
    set_effect_walls(&walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    { // Tor: type 0/1 brauchen einen Auftrag in mode1 und ein freies ev_flg 2.
        auto s=work(127,0);s.flg=1;s.mode0=5;s.px=10;s.py=1;s.pz=20;
        step_effect_sprite(s,c,&queued);
        check(s.mode0==0&&take_effect_output().spawns.empty(),"bhEff127 without an order in mode1 must reset to mode0 0");
        s.mode1=1;s.mode0=5;EffectContext blocked=c;blocked.ev_flg_2=true;
        step_effect_sprite(s,blocked,&queued);
        check(s.mode0==0&&take_effect_output().spawns.empty(),"bhEff127 must stop while ev_flg bit 2 stands");
        // type 2 laeuft ohne beides.
        auto two=work(127,2);two.flg=1;two.mode0=5;two.mode1=0;
        step_effect_sprite(two,blocked,&queued);
        check(two.mode0==5,"bhEff127 type 2 ignores mode1 and ev_flg 2");
    }
    { // type 0: Rauchsaeule, jede 16. Zaehlung, Groesse waechst bis 4.
        auto s=work(127,0);s.flg=1;s.mode1=1;s.slot=1;s.lkono=1;s.px=10;s.py=1;s.pz=20;
        step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&close(s.sx,2.0f)&&s.ct0==4&&close(effect_gas_height(),0)&&close(s.gpy,0),"bhEff127 init differs");
        for(unsigned i=0;i<11;++i)step_effect_sprite(s,c,&queued);
        check(s.ct0==15&&take_effect_output().spawns.empty(),"bhEff127 type 0 must wait for a multiple of 16");
        step_effect_sprite(s,c,&queued);
        auto out=take_effect_output();
        check(out.spawns.size()==1,"bhEff127 type 0 must place one smoke work");
        const auto& e=out.spawns[0];
        check(e.id==128&&e.flg==0x4000001u&&e.type==0&&e.flr_no==0&&e.mdlver==1,"bhEff127 smoke template differs");
        check(close(e.scale.x,2.0f)&&close(e.scale.y,0)&&close(e.scale.z,2.0f)&&close(e.position.x,10.0f)&&close(e.position.y,1.0f)&&close(e.position.z,20.0f)&&e.ax==0&&e.ay==0,"bhEff127 smoke placement differs");
        check(close(s.sx,2.5f),"bhEff127 must widen its column by 0.5");
        for(unsigned i=0;i<16*4;++i)step_effect_sprite(s,c,&queued);
        take_effect_output();
        check(close(s.sx,4.0f),"bhEff127 must stop widening at 4");
    }
    { // type 1: die Wand steigt um 0.018 je Pass bis 17 und erzeugt ab 0.5.
        auto s=work(127,1);s.flg=1;s.mode1=1;s.slot=0;s.lkono=0;s.px=0;s.py=1;s.pz=0;
        step_effect_sprite(s,c,&queued);take_effect_output();
        check(close(effect_gas_height(),0),"bhEff127 must reset sys->gas_py on its first pass");
        step_effect_sprite(s,c,&queued);
        check(close(effect_gas_height(),0.018f)&&take_effect_output().spawns.empty(),"bhEff127 gas step differs");
        unsigned passes=1;while(effect_gas_height()<=0.5f&&passes<200){step_effect_sprite(s,c,&queued);take_effect_output();++passes;}
        check(passes==28&&close(effect_gas_height(),0.504f),"bhEff127 must need 28 passes of 0.018 to pass 0.5");
        // Ab hier erzeugt jede achte Zaehlung; jede 32. nimmt den Deckenzweig.
        unsigned smoke=0,kind1=0,kind2=0;
        for(unsigned i=0;i<64;++i){step_effect_sprite(s,c,&queued);
            for(const auto& e:take_effect_output().spawns){++smoke;
                check(e.id==128&&e.flg==0x4000001u&&close(e.scale.x,50.0f)&&close(e.scale.z,50.0f)&&e.scale.y>=-1.0f&&e.scale.y<=1.0f,"bhEff127 gas smoke template differs");
                check(e.position.x>=-15.0f&&e.position.x<=15.0f&&e.position.z>=-15.0f&&e.position.z<=15.0f,"bhEff127 gas smoke scatter differs");
                if(e.type==1){++kind1;check(e.flr_no==0||e.flr_no==1,"bhEff127 ceiling tile differs");}
                else{++kind2;check(e.type==2&&e.flr_no>=2&&e.flr_no<=3,"bhEff127 cloud tile differs");}}}
        check(smoke==8&&kind1==2&&kind2==6,"bhEff127 gas cadence differs");
        // Der Spieler stirbt, sobald die Wand ueber 1 + Kopfhoehe steht.
        set_effect_link_lookup([](unsigned kind,unsigned index,unsigned node,NJS_MATRIX& out){
            if(kind||index||node!=5)return false;njUnitMatrix(&out);njTranslate(&out,0,2.0f,0);return true;});
        while(effect_gas_height()<3.5f)step_effect_sprite(s,c,&queued);
        take_effect_output();step_effect_sprite(s,c,&queued);
        check(take_effect_output().player_killed,"bhEff127 must kill the player above his head");
        // plp->stflg 0x40000 schuetzt ihn (Gasmaske).
        EffectContext safe=c;safe.player_stflg=0x40000u;
        step_effect_sprite(s,safe,&queued);
        check(!take_effect_output().player_killed,"plp->stflg 0x40000 must stop the gas death");
        // Deckel bei 17.
        for(unsigned i=0;i<1200&&effect_gas_height()<17.0f;++i){step_effect_sprite(s,c,&queued);take_effect_output();}
        step_effect_sprite(s,c,&queued);take_effect_output();
        check(effect_gas_height()<17.019f&&effect_gas_height()>=17.0f,"bhEff127 must cap sys->gas_py at 17");
    }
    { // type 2: dieselbe Wolke, aber aus der eigenen Hoehe statt aus gas_py.
        auto s=work(127,2);s.flg=1;s.slot=0;s.lkono=3;s.px=0;s.py=10.0f;s.pz=0;
        step_effect_sprite(s,c,&queued);take_effect_output();
        check(close(s.gpy,0),"bhEff127 type 2 must latch its ground height");
        unsigned smoke=0;bool high=false;
        for(unsigned i=0;i<32;++i){step_effect_sprite(s,c,&queued);
            for(const auto& e:take_effect_output().spawns){++smoke;check(e.mdlver==3,"bhEff127 type 2 mdlver differs");
                if(e.type==1)check(close(e.position.y,10.0f)||close(e.position.y,9.9f),"bhEff127 type 2 ceiling height differs");
                else{high=high||e.position.y>0.1f;check(e.position.y>=0.1f&&e.position.y<=10.0f,"bhEff127 type 2 cloud height differs");}}}
        check(smoke==2&&high,"bhEff127 type 2 cadence differs");
    }
    set_effect_walls(nullptr,0);set_effect_ground(nullptr,nullptr,nullptr);
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// effsub1.c:8720/8874 am Originalraum: RM_7210 fuehrt drei Gaswaende vom type 2,
// die ohne Skriptauftrag laufen und den Rauch 128 stellen.
static void gas_room(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_7210.RDT",64*1024*1024);
    const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_7210");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    for(unsigned i=0;i<64;++i)world.tick();
    unsigned walls=0,smoke=0,visible=0;
    for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(!s||!(s->flg&1u))continue;
        if(s->id==127)++walls;
        if(s->id==128){++smoke;if(world.effect_visible(i))++visible;}}
    check(walls==3,"RM_7210 must hold its three original gas walls");
    check(smoke>0&&visible==smoke,"the gas walls must place visible smoke works");
    Scene draw;std::vector<BoneMatrix> nodes;world.append(draw,nodes,stage.cameras.at(0));
    check(!draw.triangles.empty(),"the gas room produced no geometry");
}
// effsub5.c:1937 bhEff219 mit der Schneebibliothek effsub5.c:5579..6150.
static void group9_snow(){
    install_group4_services();
    effect_weather()=EffectWeather{};
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    const auto first_step=[&](EffectSprite& s,const EffectContext& ctx){step_effect_sprite(s,ctx,&queued);};
    { // type 3 an fester Lage: elf Hilfssaetze, Zufallsreihenfolge, Bewegung.
        recvx_core_random_seed(31);
        std::vector<float> r(300+3);for(auto& x:r)x=frac();
        recvx_core_random_seed(31);
        g_extra_next=0;g_extra_released.clear();
        auto s=work(219,3);s.flg=1;s.ax=32760;s.px=10;s.py=20;s.pz=30;s.sx=s.sy=65.0f;s.sz=4.0f;
        first_step(s,c);
        check(s.snow&&g_extra_next==11&&g_extra_released.empty(),"bhEff219 must hold eleven extra effect works (head, nine groups, vectors)");
        const auto& snow=*s.snow;
        check(snow.lSnowGrpMax==9&&snow.SnowGrp[8].lPointMax==50&&snow.SnowGrp[0].lPointMax==100&&snow.lSnowTotalMax==850,"CreateEff5SnowRect(850) group split differs");
        check(snow.header_slot==0&&snow.SnowGrp[0].slot==1&&snow.vector_slot==10,"CreateEff5SnowRect allocation order differs");
        check((s.flg&0x1000000u)&&s.mode0==1&&queued&&s.custom_queued,"bhEff219 init flags differ");
        // Vektorfeld zuerst (x, y, z je Zeile), danach die Punkte (x, y, z).
        check(close(snow.vectors[0].x,0.1f*r[0]-0.05f)&&close(snow.vectors[0].y,-(0.1f*r[1]))&&close(snow.vectors[0].z,0.1f*r[2]-0.05f),"snow vector rand order differs");
        const float ax=(10.0f-32.5f)+65.0f*r[300],ay=(20.0f-32.5f)+65.0f*r[301],az=(30.0f-32.5f)+65.0f*r[302];
        // Erster Pass: ArrangeEff5SnowRectParticle, dann ein ExecEff5SnowRect ohne Wind.
        auto wrap=[](float p,float lo,float hi,float size){if(p<lo)return hi-size*njFraction((lo-p)/size);if(hi<p)return lo+size*njFraction((p-hi)/size);return p;};
        const auto& v=snow.vectors[0];const auto& p=snow.SnowGrp[0].points[0];
        check(close(p.x,wrap(ax+v.x,-22.5f,42.5f,65.0f))&&close(p.y,wrap(ay+(-0.33f+v.y),-12.5f,52.5f,65.0f))&&close(p.z,wrap(az+v.z,-2.5f,62.5f,65.0f)),"snow first arrange/exec step differs");
        // Wind: winds * -sin(windr) / 10 (GetEff5SnowRectCurrentWindVector).
        auto& w=effect_weather();w.winds=2.0f;w.windr=16384;
        const auto before=snow.SnowGrp[0].points[1];const auto& v1=snow.vectors[1];
        step_effect_sprite(s,c,&queued);
        const auto after=snow.SnowGrp[0].points[1];
        check(close(after.x,wrap(before.x+(-0.2f+v1.x),-22.5f,42.5f,65.0f))&&close(after.z,wrap(before.z+(2.0f*-njCos(16384)/10.0f+v1.z),-2.5f,62.5f,65.0f)),"snow wind step differs");
        w=EffectWeather{};step_effect_sprite(s,c,&queued);
        // Zeichnung: jede lebende Flocke ein Billboard der Gruppe 409, Punktfilter, geerbtes Blendpaar.
        check(s.quads.size()==850,"DrawEff5SnowRect must draw every live flake");
        const auto& q=s.quads[0];
        check(q.category==409&&q.frame==0&&q.point_filter&&q.inherit_blend&&q.wind_aligned&&q.turn_y==0,"DrawEff5SnowRect quad differs");
        check(close(q.wind.x,0)&&close(q.wind.y,-0.38000003f)&&close(q.wind.z,0),"DrawEff5SnowRect wind uses y - 0.38");
        check(close(q.vertices[0].x,-(0.3f*65.0f/4.0f)/2.0f)&&close(q.vertices[3].y,(0.3f*65.0f/4.0f)/2.0f)&&q.vertices[0].col==0xa0ffffffu&&close(q.vertices[3].u,1.0f),"type 3 flake size/colour/uv differ");
        check(s.fnc_exit_src<0&&s.fnc_exit_dst<0,"DrawEff5SnowRect must leave the blend state untouched");
        // Ohne Zeichenpass wird nur bewegt.
        EffectContext dark=c;dark.pt_flg=0;step_effect_sprite(s,dark,&queued);
        check(s.quads.empty()&&queued,"bhEff219 without a draw pass enqueues but draws nothing");
    }
    { // Anteil ax/32760 und die Ueberblendung ueber 150 Passes.
        g_extra_next=0;
        auto s=work(219,3);s.flg=1;s.ax=13650;s.sx=s.sy=65.0f;
        step_effect_sprite(s,c,&queued);
        check(s.snow->lSnowExistCrnt==354&&int(s.quads.size())==354,"ax 13650 must give 354 flakes");
        s.ax=32760;step_effect_sprite(s,c,&queued);
        check(s.snow->lSnowExistPrev==354&&s.snow->lSnowExistNext==850&&s.snow->lSnowTimerMax==150&&s.snow->lSnowExistCrnt==354+(496/150),"MovEff5SnowRectParticleMax blend start differs");
        for(unsigned i=1;i<150;++i)step_effect_sprite(s,c,&queued);
        check(s.snow->lSnowExistCrnt==850,"the flake count must reach 850 after 150 passes");
        s.ax=-5;step_effect_sprite(s,c,&queued);
        check(s.ax==0&&s.snow->lSnowExistNext==0,"negative ax must clamp to 0");
    }
    { // type 2: sz wird im Zeichenteil auf 1 gehoben, Y-Drehung aus der Lage.
        EffectContext cam=c;cam.camera_ready=true;cam.camera_position={0,0,0};cam.camera_view={0,0,1};
        g_extra_next=0;
        auto s=work(219,2);s.flg=1;s.ax=32760;s.sx=4;s.sy=3;s.sz=0.25f;
        step_effect_sprite(s,cam,&queued);
        check(close(s.sz,1.0f),"DrawEff5SnowRect type 2 must raise sz to 1");
        check(close(s.snow->AreaCenter.z,40.0f)&&close(s.snow->fAreaSizeX,65.0f),"type 2 box must sit 40 ahead of the camera");
        check(close(s.quads[0].vertices[1].x,(0.3f*4.0f/4.0f)/2.0f)&&close(s.quads[0].vertices[2].y,(0.3f*3.0f/4.0f)/2.0f),"type 1/2 flake size uses sx/sy");
        bool turned=false;std::size_t qi=0;
        for(int g=0;g<s.snow->lSnowGrpMax;++g)for(int k=0;k<s.snow->SnowGrp[std::size_t(g)].lPointMax;++k,++qi){
            const auto& p=s.snow->SnowGrp[std::size_t(g)].points[std::size_t(k)];
            check(s.quads[qi].turn_y==(int((p.z+(p.x+p.y))/1.0f)%4)*16384,"type 2 turn differs");
            turned=turned||s.quads[qi].turn_y<0;}
        check(turned,"type 2 must keep the negative remainders of %");
        // type 4 faellt per (type-1)%3+1 auf 1.
        auto four=work(219,4);four.flg=1;four.ax=100;g_extra_next=0;step_effect_sprite(four,cam,&queued);
        check(four.type==1,"bhEff219 type must wrap to 1..3");
    }
    { // Kamerabox ohne gelaufenes bhControlCamera: cam ist beim Raumladen genullt
      // (room.c:33 bhInitCamera, camera.c:8 npSetMemory(&cam, 0x4AC, 0)) und game.c:50
      // bhControlEffect laeuft vor :58 bhControlCamera — die Box liegt wie im Original
      // am Ursprung und folgt der Kamera, sobald sie gelaufen ist. Kein Warten, keine Luecke.
        g_extra_next=0;take_effect_output();
        auto s=work(219,1);s.flg=1;s.ax=100;step_effect_sprite(s,c,&queued);
        auto out=take_effect_output();
        check(s.snow&&g_extra_next==11&&out.gaps.empty()&&s.mode0==1&&queued,"bhEff219 without a camera pass must place the box at the zeroed cam like the original");
        check(close(s.snow->AreaCenter.x,0)&&close(s.snow->AreaCenter.y,0)&&close(s.snow->AreaCenter.z,0)&&close(s.snow->fAreaSizeX,65.0f),"the camera box without a camera pass must sit at the origin with edge 65");
        // Ohne Auftrag (type 0, mode1 0) bleibt der Satz unberuehrt.
        auto idle=work(219,0);idle.flg=1;step_effect_sprite(idle,c,&queued);
        check(!idle.snow&&!(idle.flg&0x1000000u)&&!queued,"bhEff219 without an order does nothing");
    }
    { // Voller Pool: was schon belegt war, geht zurueck, der Satz endet.
        int budget=5;g_extra_next=0;g_extra_released.clear();
        set_effect_extra_allocator([&budget](){return budget-->0?g_extra_next++:-1;},[](int slot){g_extra_released.push_back(slot);});
        auto s=work(219,3);s.flg=1;s.ax=32760;s.sx=s.sy=65;step_effect_sprite(s,c,&queued);
        std::sort(g_extra_released.begin(),g_extra_released.end());
        check(s.flg==0&&!s.snow&&g_extra_released==std::vector<int>({0,1,2,3,4}),"a failed CreateEff5SnowRect must release every work it took");
        install_group4_services();
    }
    { // Blendzustand der ef_fnc-Liste: bhDraw107 laesst 8/10 stehen, die Schneeflocken erben ihn.
        g_extra_next=0;
        auto s=work(219,3);s.flg=1;s.ax=328;s.sx=s.sy=65;step_effect_sprite(s,c,&queued);
        check(s.quads.size()==8,"ax 328 must give eight flakes");
        std::map<unsigned,std::pair<unsigned,unsigned>> bases{{409,{0,1}}};
        color_blending_mode(0,8);color_blending_mode(1,10);const auto additive=color_blending_alpha();
        color_blending_mode(0,8);color_blending_mode(1,6);const auto normal=color_blending_alpha();
        SceneCamera camera;
        Scene a;std::pair<unsigned,unsigned> state{8,6};append_custom_effect(a,s,camera,bases,&state);
        check(!a.triangles.empty()&&a.triangles[0].blend_register==normal&&state==std::pair<unsigned,unsigned>(8,6),"snow after a restoring function draws with 8/6");
        Scene b;state={8,10};append_custom_effect(b,s,camera,bases,&state);
        check(b.triangles[0].blend_register==additive&&state==std::pair<unsigned,unsigned>(8,10),"snow after bhDraw107 must inherit 8/10 and keep it");
        EffectSprite rain;rain.fnc_exit_src=8;rain.fnc_exit_dst=10;state={8,6};Scene none;append_custom_effect(none,rain,camera,bases,&state);
        check(state==std::pair<unsigned,unsigned>(8,10),"bhDraw107 leaves 8/10 even without a quad");
        // Winddrehung unter cam.mtx: Einheitskamera, Wind (0,-0.38,0) -> 32768 um Z.
        const auto& q=s.quads[0];const auto& t=a.triangles[0];
        const float hx=-q.vertices[0].x,hy=-q.vertices[0].y;
        check(close(t.vertices[0].position.x,q.center.x+hx)&&close(t.vertices[0].position.y,q.center.y-hy),"snow flake wind turn differs");
    }
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// Originalraum RM_9090: zwei Schneeboxen vom type 3 (Eintraege 24/25) und eine
// Kamerabox (12). Ohne Kameralauf liegt die Kamerabox wie im Original am genullten cam.
// Der Test laeuft ohne Raumskript: im Spiel schaltet das Skript alle drei Boxen beim
// Betreten per 0x43 bhEffDispSet (event.c:3653) aus. Er prueft den Effektpfad, nicht
// die Sichtbarkeit im Raum; die Bildabnahme steht im Ledger (RM_5840).
static void snow_room(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_9090.RDT",64*1024*1024);
    const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_9090");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    for(unsigned i=0;i<4;++i)world.tick();
    unsigned extra=0,boxes=0;
    for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(!s)continue;if((s->flg&3u)==2u)++extra;if(s->id==219&&s->snow)++boxes;}
    check(boxes==3&&extra>=33,"RM_9090 must hold three snow boxes (two fixed, one camera) in 33 extra works");
    Scene draw;std::vector<BoneMatrix> nodes;world.append(draw,nodes,stage.cameras.at(0));
    const auto flakes=std::count_if(draw.triangles.begin(),draw.triangles.end(),[](const SceneTriangle& t){return t.color==0xa0ffffffu&&t.texture_filter==SceneTextureFilter::Point;});
    check(flakes>0&&flakes%2==0,"the snow boxes produced no flakes in the scene");
    // Die Kamerabox (Eintrag 12, type 1) laeuft mit, ohne eine Luecke zu melden; nur die
    // beiden type-3-Boxen stehen fest, die Kamerabox haengt an cam.wp + 40 * cam.v.
    unsigned fixed=0,camera_box=0;
    for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(!s||s->id!=219||!s->snow)continue;
        if(s->type==3)++fixed;else{++camera_box;check(close(s->snow->fAreaSizeX,65.0f),"the camera box must keep edge 65");}}
    check(fixed==2&&camera_box==1,"RM_9090 must hold two fixed boxes and one camera box");
    for(const auto& gap:world.unbound_enemy_routines())check(gap.rfind("bhEff219",0)!=0,"bhEff219 must not report a gap on the zeroed camera");
}
// effsub1b.c:621/713 bhEff135/136 und effect.c:935 ef_Zanzo, mit Testdiensten.
static void group10_afterimages(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    bool opened=false;std::size_t used=0;int vertices=100;unsigned captures=0;
    EffectAfterimageServices svc;
    svc.zan_open=[&](){opened=true;};
    svc.zan_reserve=[&](std::size_t bytes){if(used+bytes>=0x200000u)return false;used+=bytes;return true;};
    svc.vertex_count=[&](unsigned kind,unsigned index,unsigned mdl,unsigned node){return kind==1&&index==2&&mdl==0&&node==1?vertices:-2;};
    svc.work_position=[](unsigned,unsigned)->std::optional<NJS_POINT3>{return NJS_POINT3{1,2,3};};
    svc.capture=[&](unsigned kind,unsigned index,unsigned mdl,unsigned node)->std::shared_ptr<const EffectAfterimage>{
        ++captures;auto a=std::make_shared<EffectAfterimage>();a->kind=kind;a->index=index;a->mdl=mdl;a->node=node;a->triangles.resize(3);return a;};
    set_effect_afterimage_services(svc);
    { // 135: ohne Auftrag nur ef_flg/zan, mit Auftrag und Link die Reservierung.
        auto s=work(135,0);s.flg=1u|0x80u;s.slot=7;s.link_kind=1;s.link_index=2;s.lkono=1;
        step_effect_sprite(s,c,&queued);
        check(opened&&(s.flg&0x1000000u)&&s.mode0==0&&used==0&&!queued,"bhEff135 without an order only opens the buffer");
        s.mode1=1;step_effect_sprite(s,c,&queued);
        // ((100 << 5) + 72) = 3272, +64 - 3272 % 64 = 3328; acht Plaetze.
        check(s.mode0==1&&s.ct0==0&&used==8*3328u&&take_effect_output().spawns.empty(),"bhEff135 reservation size differs");
        // Ein glatter Rest bekommt trotzdem einen vollen 64er-Schritt: (126 << 5) + 72 = 4104, Rest 8 -> 4160;
        // (2 << 5) + 72 = 136 -> 192; 0 Eckpunkte: 72 -> 128.
        auto t=work(135,0);t.flg=1u|0x80u;t.mode1=1;t.link_kind=1;t.link_index=2;t.lkono=1;vertices=0;used=0;
        step_effect_sprite(t,c,&queued);check(used==8*128u,"bhEff135 rounds 72 up to 128");
        // Passt der Puffer nicht mehr, bleibt mode0 0 und der naechste Pass versucht es erneut.
        auto full=work(135,0);full.flg=1u|0x80u;full.mode1=1;full.link_kind=1;full.link_index=2;full.lkono=1;vertices=100;used=0x200000u-8*3328u;
        step_effect_sprite(full,c,&queued);check(full.mode0==0&&used==0x200000u-8*3328u,"bhEff135 must not reserve past 2 MB (>= test)");
        // model NULL: still; unbekanntes Modell: Luecke.
        auto none=work(135,0);none.flg=1u|0x80u;none.mode1=1;none.link_kind=1;none.link_index=2;none.lkono=1;vertices=-1;used=0;
        step_effect_sprite(none,c,&queued);check(none.mode0==0&&used==0&&take_effect_output().gaps.empty(),"bhEff135 with a NULL model stays silent");
        none.lkono=4;step_effect_sprite(none,c,&queued);check(take_effect_output().gaps.size()==1,"bhEff135 with an unknown model reports the gap");
        // Ohne flg 0x80 (bhSetEffectLink fand das Werk nicht aktiv) passiert nichts.
        auto loose=work(135,0);loose.flg=1;loose.mode1=1;loose.link_kind=1;loose.link_index=2;loose.lkono=1;vertices=100;used=0;
        step_effect_sprite(loose,c,&queued);check(loose.mode0==0&&used==0,"bhEff135 needs flg 0x80");
        // mode0 1: je Pass ein 136, Ringplatz 0..5, an dasselbe Werk, flg 0x80 wieder geloescht.
        for(unsigned pass=0;pass<7;++pass){step_effect_sprite(s,c,&queued);auto out=take_effect_output();
            check(out.spawns.size()==1,"bhEff135 must place one afterimage per pass");
            const auto& e=out.spawns[0];
            check(e.id==136&&e.flg==1&&e.type==7&&e.flr_no==int(pass%6)&&e.mdlver==0&&e.lkono==1&&e.link_kind==1&&e.link_index==2&&e.drop_link_flag,"bhEff136 request differs");
            check(close(e.position.x,1)&&close(e.position.y,2)&&close(e.position.z,3),"bhEff136 starts at pp->px");}
        s.mode1=0;step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty()&&s.mode0==1,"bhEff135 without an order places nothing");
    }
    { // 136: Schnappschuss, dann fuenf gezeichnete Passes mit Alpha 200..40, dann frei.
        EffectSpawn spawn;spawn.id=136;spawn.flg=1;spawn.type=7;spawn.lkono=1;spawn.link_kind=1;spawn.link_index=2;
        EffectSprite s;initialize_effect_sprite(s,spawn);s.link_kind=1;s.link_index=2;
        captures=0;step_effect_sprite(s,c,&queued);
        check(captures==1&&s.afterimage&&s.afterimage->node==1&&s.mode0==1&&s.ct0==5&&!queued,"bhEff136 snapshot pass differs");
        const unsigned alphas[5]={200,160,120,80,40};
        for(unsigned pass=0;pass<5;++pass){step_effect_sprite(s,c,&queued);
            check(queued&&s.zanzo_slot==int(5-pass)&&s.afterimage_alpha==alphas[pass]&&!s.custom_queued,"bhEff136 Zanzo pass differs");}
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0&&s.zanzo_slot<0,"bhEff136 must end after five drawn passes");
        // Raum 1-00 Fall 2 (sowie 5-82, 9-32 Fall 1): die ef_fnc-Liste statt ef_Zanzo.
        EffectContext fnc=c;fnc.stage_no=1;fnc.room_no=0;fnc.room_case=2;
        initialize_effect_sprite(s,spawn);s.link_kind=1;s.link_index=2;step_effect_sprite(s,fnc,&queued);step_effect_sprite(s,fnc,&queued);
        check(queued&&s.custom_queued&&s.zanzo_slot<0&&s.afterimage_alpha==200,"bhEff136 in room 1-00/2 draws through ef_fnc");
        fnc.room_case=1;initialize_effect_sprite(s,spawn);s.link_kind=1;s.link_index=2;step_effect_sprite(s,fnc,&queued);step_effect_sprite(s,fnc,&queued);
        check(s.zanzo_slot==5,"room 1-00 case 1 keeps ef_Zanzo");
        // Ohne Schnappschuss: Luecke, kein Nachbild.
        svc.capture=[](unsigned,unsigned,unsigned,unsigned)->std::shared_ptr<const EffectAfterimage>{return nullptr;};set_effect_afterimage_services(svc);
        initialize_effect_sprite(s,spawn);s.link_kind=1;s.link_index=2;take_effect_output();step_effect_sprite(s,c,&queued);
        check(s.flg==0&&take_effect_output().gaps.size()==1,"bhEff136 without native geometry reports the gap");
    }
    set_effect_afterimage_services({});
    set_effect_extra_allocator({},{});set_effect_link_lookup({});set_effect_work_angle({});set_effect_pool_lookup({});
}
// RM_7070 (Gegner 5, Gelenk 1, ef_Zanzo) und RM_1002 (Fall 2, ef_fnc): Testaufstellung,
// die die 135-Eintraege wie Skriptbefehl 0x91 einschaltet. Geprueft wird die Kette
// bhSetEffectLink -> 135 -> 136 -> Nachbild in der Szene auf echten Raumdaten.
// effect.c:905 bhDrawNtxEffect2D: der Schirmsatz von bhEff130 type 3 (BOW-Gas) faerbt das
// fertige Bild. Erwartung aus ps2_NaDraw.c:8 njDrawPolygon: Alpha = col >> 25 (a >> 1) als
// GS-Verhaeltnis / 128 unter 8/6, Eckfarbe 0x10C040; auf schwarzem Grund also Kanal*ratio/128.
// Tore: pt_flg 0x10 (effect.c:339), flg/stflg 0x1000000 und gm_flg 0x4000 + mdflg 0x40.
static void ntx2d_room(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_5840.RDT",64*1024*1024);
    const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_5840");
    world.set_simulation_flags(~0u);
    const int slot=world.reserve_effect_slot(0);check(slot>=0,"no free eff[] slot for the gas screen");
    auto& s=const_cast<EffectSprite&>(*world.effect_state(unsigned(slot)));
    // Ein BOW-Gas-Geschoss im Einschlag (mode0 2, type 3, mode1 0): der erste Pass legt den
    // Schirm-Vierecksatz pv an (:9820-:9834); erst ab ct0 > 7 steht er in ef_ntx2d (:9904).
    s.flg=1;s.id=130;s.type=3;s.mode0=2;s.mode1=0;s.px=0;s.py=0;s.pz=0;
    auto draw=[&](unsigned pt){world.set_presentation_flags(pt);SoftwareRenderer image(640,480);std::fill(image.canvas().begin(),image.canvas().end(),0xff000000u);world.draw_ntx2d_effects(image);return image.canvas();};
    const auto all_black=[](const std::vector<std::uint32_t>& c){return std::all_of(c.begin(),c.end(),[](std::uint32_t p){return p==0xff000000u;});};
    const auto& w=*world.effect_state(unsigned(slot));
    unsigned passes=0,first_listed=0;
    while(!w.ntx2d_queued&&passes<40){world.tick();++passes;check(all_black(draw(0x10)),"an unlisted gas screen must not tint the frame");}
    first_listed=passes;
    check(w.mode1==1&&w.ct0==9,"first ef_ntx2d entry must be the pass with ct0 8 (:9904), leaving ct0 9");
    check(close(w.pv[0],0)&&close(w.pv[4],640.0f)&&close(w.pv[9],480.0f)&&close(w.pv[14],1.2195f)&&w.pn==4,"screen quad pv (0,0)-(640,480), z 1.2195 (:9820-:9834)");
    check(all_black(draw(0x10)),"sin(0) screen must not tint the frame");
    // Bis zum letzten reinen Phase-1-Eintrag (ct0 23 -> 24): Alpha 64*sin(15<<10) = 63, ratio 31.
    while(w.ct0<24&&w.mode1==1)world.tick();
    check(w.ntx2d_queued&&w.mode1==1&&w.ct0==24,"gas screen pass count differs");
    std::uint32_t col;std::memcpy(&col,&w.pv[3],4);
    const unsigned a=unsigned(int(64.0f*njSin(Angle(15<<10))))&0xFFu;
    check(a==63&&col==((std::uint32_t(a)<<24)|0x10C040u),"gas screen colour in the last pure phase-1 pass differs");
    auto tinted=draw(0x10);
    // njDrawPolygon: Alpha = col >> 25 = a >> 1 als GS-Verhaeltnis / 128; ueber Schwarz also Kanal*ratio/128.
    const unsigned ratio=(a>>1);
    const unsigned want_r=(0x10u*ratio)/128u,want_g=(0xC0u*ratio)/128u,want_b=(0x40u*ratio)/128u;
    const std::uint32_t want=0xff000000u|(want_r<<16)|(want_g<<8)|want_b;
    unsigned matches=0;for(auto p:tinted)if(p==want)++matches;
    check(matches>=640u*480u*99u/100u,"gas screen tint differs from njDrawPolygon alpha (col >> 25)/128 over the full frame");
    // Tore: ohne pt_flg 0x10 kein Effektzeichenpass; stflg 0x1000000 versteckt den Satz.
    check(all_black(draw(0)),"ntx2d drew without pt_flg 0x10");
    s.stflg|=0x1000000u;const bool hidden_black=all_black(draw(0x10));s.stflg&=~0x1000000u;
    check(hidden_black,"ntx2d drew a work hidden by stflg 0x1000000");
    // Das Zeichnen selbst aendert den Satz nicht (const Pass).
    check(w.ntx2d_queued&&w.mode1==1&&w.ct0==24,"draw pass mutated the gas screen work");
    // :9904-:9916 im naechsten Pass: Eintrag mit ct0 24 (Alpha 64, Scheitel), ct0++ -> 25, > 24 ->
    // bhSetBowDamage, ct0 32, mode1 2 — alles in einem Pass; der Eintrag bleibt bis zum naechsten Pass.
    world.tick();
    check(w.mode1==2&&w.ct0==32&&w.ntx2d_queued,"the bow-damage pass still lists the peak screen (:9908 before :9913)");
    std::memcpy(&col,&w.pv[3],4);check(col==((64u<<24)|0x10C040u),"peak colour must be alpha 64");
    // :9924-:9934 Ausblenden: 31 Paesse mit Eintrag (ct0 31..1), der Pass mit ct0 1 -> 0 gibt frei ohne Eintrag.
    unsigned fade_listed=0;for(int i=0;i<40&&w.flg;++i){world.tick();if(w.flg&&w.ntx2d_queued)++fade_listed;}
    check(!w.flg&&fade_listed==31,"fade phase must list 31 passes, then free the work");
    (void)first_listed;
}
static void afterimage_room(const std::filesystem::path& data){
    for(const char* name:{"RM_7070","RM_1002"}){
        const auto room=read_asset_file(data/(std::string("rooms/")+name+".RDT"),64*1024*1024);
        const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(room,stage,actor,motion,frame,audio,data,{},name);
        auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
        Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);
        events.values[21]=RoomIdentity::parse(name).variant; /* runtime.cpp:384 sys->rcase */
        world.bind_player_collision(events);
        for(unsigned i=0;i<4;++i){world.tick();world.tick_camera(stage,stage.cameras.at(0),0);}
        unsigned armed=0;
        for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&s->id==135){
            check((s->flg&0x80u)&&s->link_kind>=1,"bhSetEffectLink must link the room's 135 entries");
            const_cast<EffectSprite*>(s)->mode1=1;++armed;}}
        check(armed>0,"room without its 135 entries");
        for(unsigned i=0;i<12;++i){world.tick();world.tick_camera(stage,stage.cameras.at(0),0);}
        unsigned shots=0,zanzo=0,fnc=0;
        for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(!s||!(s->flg&1u)||s->id!=136)continue;
            check(!(s->flg&0x80u)&&s->afterimage,"bhEff136 must hold a snapshot and no flg 0x80");++shots;if(s->zanzo_slot>=0)++zanzo;if(s->custom_queued)++fnc;}
        // RM_1002 Eintrag 17 haengt an obwp[17] Gelenk 0 ohne Modell (objP[0].model NULL):
        // er bleibt wie im Original still in mode0 0.
        unsigned producing=0;for(unsigned i=0;i<512;++i){const auto* s=world.effect_state(i);if(s&&s->id==135&&s->mode0==1)++producing;}
        check(producing==(std::string(name)=="RM_1002"?2u:1u),"the 135 entries with a model must reserve their snapshots");
        check(shots==6*producing,"every producing 135 keeps six afterimages alive");
        const bool fnc_room=std::string(name)=="RM_1002";
        check(fnc_room?(fnc==5*producing&&zanzo==0):(zanzo==5&&fnc==0),"afterimage list (ef_fnc/ef_Zanzo) differs");
        Scene draw;std::vector<BoneMatrix> nodes;world.append(draw,nodes,stage.cameras.at(0));
        // Die Nachbilder liegen in der Effektphase 6 (ef_fnc/ef_Zanzo, world.cpp append_afterimage).
        // Halbes Licht allein genuegt als Merkmal nicht: objitm.c:518-529 zeichnet ob_hlg/ob_spc
        // nach bhSetHalfLight() mit amb_chr, also ebenfalls in Kategorie 1 (Phasen 3/4).
        std::map<unsigned,std::size_t> alphas;
        for(const auto& t:draw.triangles)if(t.light_category==1&&!t.after_modifiers&&t.gouraud&&t.blend==SceneBlend::Ninja&&t.material_phase==6){
            if(t.diffuse_material)++alphas[t.color>>24];
            for(const auto& v:t.vertices)check(v.light.x>=0&&v.light.x<=1.0f+1e-4f,"afterimage lit RGB outside native range");}
        check(alphas.size()==5&&alphas.count(40)&&alphas.count(200),"afterimages must draw with material alpha 200..40");
        // Jede Stufe ist dasselbe Modell in einem anderen Pass, also dieselbe Dreieckszahl.
        for(const auto& [alpha,count]:alphas)check(count==alphas.begin()->second,"every afterimage pass must draw the same model");
        // objitm.c:518-529: die Objektlisten ob_hlg/ob_spc gehoeren auf halbes Licht.
        for(const auto& t:draw.triangles)if(t.material_phase==3||t.material_phase==4)
            check(t.light_category==1,"ob_hlg/ob_spc must draw at half light (objitm.c:518-529)");
        check(world.unbound_enemy_routines().count("bhEff135: pp->mdl[type].objP[lkono].model ohne nativen Modellbesitzer")==0,"afterimage source model unresolved");
    }
}

// effsub2.c:623 bhEff_E00_AcidGenerator (298) — die Quelle, die 257 nachschiebt.
// Erzeuger ist en19.c:3336 bhEne19_SetDmgEffect Zeile 5 {2, 131072, 32, 298}.
static void acid_generator(){
    install_group4_services();
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    // Die Erzeugerzeile: scl = 0.000015258789f * 131072, type = 32 als Zahl der Ausgaben.
    const float scl=0.000015258789f*131072.0f;
    const auto spawn298=[&](){
        EffectSpawn e;e.flg=1;e.id=298;e.type=32;e.mdlver=0;e.position={3.0f,4.0f,5.0f};
        e.scale={scl,scl,scl};e.ax=0;e.ay=0;
        EffectSprite s;initialize_effect_sprite(s,e);return s;};
    { // Wartezeit: ct0 = 5 und `ct0-- == 0` je Pass -> sechs Paesse ohne Ausgabe.
        auto s=spawn298();
        step_effect_sprite(s,c,&queued);
        check(s.mode0==1&&s.ct0==4&&!queued&&take_effect_output().spawns.empty(),"bhEff_E00_AcidGenerator :635: ct0 = 5, erster Pass ohne Ausgabe");
        unsigned passes=1;
        while(s.mode0==1&&passes<20){step_effect_sprite(s,c,&queued);++passes;check(take_effect_output().spawns.empty(),"298 gibt waehrend der Wartezeit nichts aus");}
        check(passes==6&&s.mode0==2,"bhEff_E00_AcidGenerator :639: sechs Paesse bis mode0 2");
        // Ab mode0 2 je Pass genau zwei 257: eigener Platz und ein versetzter mit 0.4-facher Groesse.
        recvx_core_random_seed(23);float r[3];for(auto& x:r)x=frac();recvx_core_random_seed(23);
        step_effect_sprite(s,c,&queued);
        auto out=take_effect_output();
        check(out.spawns.size()==2,"bhEff_E00_AcidGenerator :663/:676: zwei 257 je Pass");
        check(!queued,"298 zeichnet selbst nichts (kein ef_trs)");
        const auto& a=out.spawns[0];const auto& b=out.spawns[1];
        check(a.id==257&&a.flg==1&&a.type==0&&a.ax==0&&a.ay==0,"298 :647-:661: erster 257 differiert");
        check(close(a.position.x,3.0f)&&close(a.position.y,4.0f)&&close(a.position.z,5.0f),"298 :652: erster 257 an der eigenen Lage");
        check(close(a.scale.x,scl)&&close(a.scale.y,scl)&&close(a.scale.z,scl),"298 :656: erster 257 mit der eigenen Groesse");
        check(b.id==257&&b.flg==1&&b.type==0&&b.ax==0&&b.ay==0,"298 zweiter 257 differiert");
        // :665-:667 ein Zufallswert je Achse, Reihenfolge x, y, z.
        check(close(b.position.x,3.0f+(0.5f*(scl*(r[0]-0.5f))))&&close(b.position.y,4.0f+(0.5f*(scl*(r[1]-0.5f))))&&close(b.position.z,5.0f+(0.5f*(scl*(r[2]-0.5f)))),"298 :665-:667: Streuung oder Zufallsreihenfolge differiert");
        check(close(b.scale.x,0.4f*scl)&&close(b.scale.y,0.4f*scl)&&close(b.scale.z,0.4f*scl),"298 :669-:671: zweiter 257 mit 0.4-facher Groesse");
        check(s.type==31,"298 :680: type zaehlt je Ausgabe abwaerts");
    }
    { // type 32 heisst 32 ausgebende Paesse; danach ist der Satz frei.
        auto s=spawn298();
        for(unsigned i=0;i<6;++i)step_effect_sprite(s,c,&queued);
        take_effect_output();
        unsigned emits=0;std::size_t works=0;
        while(s.flg&&emits<100){step_effect_sprite(s,c,&queued);works+=take_effect_output().spawns.size();++emits;}
        check(emits==32&&works==64&&s.type==0&&s.flg==0,"bhEff_E00_AcidGenerator :678-:686: 32 Ausgaben, dann flg 0");
    }
    { // :630 Der verlinkte Erzeuger mit stflg 0x1000000 haelt das ganze Werk an, auch den Zaehler.
        RECVX_ENEMY_WORK owner{};owner.stflg=0x1000000u;
        auto s=spawn298();s.lkwkp=&owner;
        for(unsigned i=0;i<10;++i)step_effect_sprite(s,c,&queued);
        check(s.mode0==0&&s.ct0==0&&take_effect_output().spawns.empty(),"298 :630: versteckter Erzeuger haelt auch den Wartezaehler an");
        owner.stflg=0;
        step_effect_sprite(s,c,&queued);
        check(s.mode0==1&&s.ct0==4,"298: nach dem Freigeben laeuft die Wartezeit von vorn an");
        // Ein Erzeuger, der mitten im Nachschieben verschwindet, gibt nichts mehr aus.
        for(unsigned i=0;i<6;++i)step_effect_sprite(s,c,&queued);
        check(s.mode0==2,"298: mode0 2 nach der Wartezeit");
        take_effect_output();
        const unsigned left=s.type;
        owner.stflg=0x1000000u;
        for(unsigned i=0;i<5;++i)step_effect_sprite(s,c,&queued);
        check(s.type==left&&take_effect_output().spawns.empty(),"298: versteckter Erzeuger stoppt die Ausgabe ohne type zu verbrauchen");
    }
    // Ohne Link (lkwkp NULL) laeuft der Satz wie im Original (owp == NULL ist der erste Zweig).
    {
        auto s=spawn298();
        for(unsigned i=0;i<7;++i)step_effect_sprite(s,c,&queued);
        check(s.mode0==2&&take_effect_output().spawns.size()==2,"298 :630: ohne lkwkp laeuft der Satz");
    }
}

static void afterimage_color_contract(){
    // ps2_Vu1Strip.c:1654 DifAmb (selected by flags 0x0A): clamp first, then material.
    const auto dark=cnk_afterimage_color(0xffc86432u,0);
    check(close(dark.x,160)&&close(dark.y,80)&&close(dark.z,40),"DifAmb must multiply material after adding ambient");
    const auto half=cnk_afterimage_color(0xffc86432u,0.1f);
    check(close(half.x,180)&&close(half.y,90)&&close(half.z,45),"DifAmb light/ambient order differs");
    const auto saturated=cnk_afterimage_color(0xffc86432u,0.7f);
    check(close(saturated.x,200)&&close(saturated.y,100)&&close(saturated.z,50),"DifAmb must clamp the light before multiplying diffuse RGB");
}
static void published_afterimage_models(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_7070.RDT",64*1024*1024);const auto stage=decode_scene(room);auto actor=stage.spawns.at(0);
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_7070");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);world.tick();
    // Test publication at a non-bind pose. Hide the selected node and its ancestor:
    // bhDraw136 draws the selected model directly, not the visible hierarchy.
    auto pose=player.local_pose(42,0);auto flags=player.node_evalflags();unsigned node=0;
    for(;node<player.bones();++node)if(model_object_vertex_count(player.encoded_model(),0,node).has_value())break;
    check(node<player.bones(),"player has no snapshot model");
    NJS_MATRIX root;njUnitMatrix(&root);njTranslate(&root,100,20,30);njRotateY(&root,8192);
    auto matrices=player.tree_matrices(pose,root);flags[0]|=8u;flags[node]|=8u;
    world.commit_afterimage_model(0,0,0,player,pose,matrices,flags);
    const auto& svc=effect_detail::afterimage_service();
    const auto logical=player.script_index(node);const auto shot=svc.capture(0,0,0,logical);
    check(shot&&!shot->triangles.empty()&&!shot->textures.empty(),"committed player object must capture even under hidden nodes");
    auto expected=player.sample_object_work(node,pose,matrices,flags);
    check(shot->triangles.size()==expected.triangles.size(),"player snapshot must contain exactly one object's model");
    for(unsigned k=0;k<3;++k){const auto a=shot->triangles.front().vertices[k].position,b=expected.triangles.front().vertices[k].position;
        check(close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z),"player snapshot differs from committed owP");}
    motion=0;frame=123*65536;actor.position={-100,300,800};
    const auto frozen=svc.capture(0,0,0,logical);
    check(frozen&&close(frozen->triangles[0].vertices[0].position.x,shot->triangles[0].vertices[0].position.x),"snapshot must not rebuild the next player motion or world position");
    // An additional mdl[n] owns its own texture/objP/owP. Publish one that has geometry.
    // mdl[1] is a modifier volume, not a textured body model. mdl[4] is the
    // control-owned Claire attachment (bhResetPlayer), used by the runtime too.
    unsigned extra=4,extra_node=0;const PlayerAsset* model=player.model(extra);
    check(model,"player mdl[4] attachment missing");
    for(;extra_node<model->bones();++extra_node)if(model_object_vertex_count(model->encoded_model(),0,extra_node))break;
    check(extra_node<model->bones(),"player attachment has no model object");
    const auto extra_logical=model->script_index(extra_node);
    check(svc.vertex_count(0,0,extra,extra_logical)>=0,"bhEff135 must inspect mdl[n] vertex count");
    // Assets alone do not justify guessing rest matrices.
    take_effect_output();check(!svc.capture(0,0,extra,extra_logical),"an unpublished supplementary model must remain a gap");
    auto ep=model->bind_pose();auto ef=model->node_evalflags();njUnitMatrix(&root);njTranslate(&root,40,50,60);
    auto em=model->tree_matrices(ep,root);world.commit_afterimage_model(0,0,extra,*model,ep,em,ef);
    const auto es=svc.capture(0,0,extra,extra_logical);
    check(es&&!es->triangles.empty()&&es->mdl==extra,"published supplementary mdl must capture");
    const auto ee=model->sample_object_work(extra_node,ep,em,ef);
    check(close(es->triangles[0].vertices[0].position.y,ee.triangles[0].vertices[0].position.y),"additional model used body matrices instead of its own owP");
    world.set_player_asset(nullptr);
    check(!svc.capture(0,0,0,logical),"rebinding the player must discard published model handles");
    set_effect_afterimage_services({});
}
// effsub5.c:3036 bhEff229 / :3117 bhDrawWeaponEffect2 — Muendungsfeuer der Doppelwaffe
// (WpnEffTab[21][0]: flg 0x81, flr_no 3, mdlver 0, Skalierung 4). Drei Passes: ein
// zentrales Quad und zwei Balken (Blend 8/10), danach sechs um Z gedrehte Balkenpaare
// (Blend 8/6) im Gelenkraum des verlinkten Werks (Texturgruppe 3). Die Eckpunkte werden
// ueber op->mtx = lkwkp->owP[lkono] * [lx,ly,lz] * ax/ay/az und die Skalierung (sx,sx,sz)
// in Weltkoordinaten gerechnet.
static void muzzle_weapon229(){
    EffectContext c;bool queued=false;auto s=work(229,0);
    s.flg=0x81u;s.flr_no=3;s.type=0;s.mdlver=0;s.sx=s.sy=s.sz=4.0f;
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,10.0f,20.0f,30.0f);
    float link[16];std::memcpy(link,m,sizeof link);s.link_matrix=link;
    for(unsigned pass=0;pass<3;++pass){
        check(step_effect_sprite(s,c,&queued)&&queued,"bhEff229 did not queue a draw");
        check(s.custom_queued,"bhEff229 is not an ef_fnc (custom) sprite");
        check(s.tex_id==3&&s.bl_src==8&&s.bl_dst==10,"bhEff229 texture/blend differs");
        check(s.spd==(pass==1?1.0f:0.8f),"bhEff229 spd differs from ct0");
        check(s.quads.size()==15,"bhEff229 emitted an unexpected number of quads (expected 15)");
        for(const auto& q:s.quads){
            check(q.world_space&&q.textured&&q.category==3&&q.pn==4,"bhEff229 quad flags differ");
            check(q.frame<2,"bhEff229 texture frame outside akpal[2][3]");
        }
        check(s.quads[0].source==8&&s.quads[0].destination==10,"bhEff229 central quad blend differs");
        check(s.quads[3].source==8&&s.quads[3].destination==6,"bhEff229 turned pair blend differs");
    }
    // Erstes Quad (Eckpunkt 0): skalierte Ecke (-0.25,-0.25,0.3)*4 = (-1,-1,1.2), op->mtx T(10,20,30).
    check(close(s.quads.empty()?0:s.quads[0].vertices[0].x,9.0f),"bhEff229 first vertex x differs");
    check(s.mode0==2&&s.ct0==3,"bhEff229 did not finish after flr_no passes");
    check(step_effect_sprite(s,c,&queued)&&s.flg==0&&!queued,"bhEff229 did not end after its passes");
}
// effsub4.c:654 bhEff_Sub352 — Flamme am verlinkten Werk (Erzeuger ist das Gegner-Feuer
// bhEne_SetSanEffect/zonzon.c:939: zn=len, ct3=time, shp_ct=Bodenhoehe). Tor pt_flg 0x2;
// sxb waechst um yn bzw. schrumpft x0.96; die Weltlage ist Werk-Gelenk * (ofx,ofy,ofz),
// um cam.v * zn zur Kamera verschoben, als lokale lox/loy/loz gehalten.
static void flame352(){
    EffectContext off;off.pt_flg=1u;bool q=false;auto s2=work(352,0);
    check(step_effect_sprite(s2,off,&q)&&!q,"bhEff352 runs without pt_flg 0x2");

    EffectContext c;c.pt_flg=0x2u;c.camera_view={0,0,-1};bool queued=false;
    auto s=work(352,0);
    s.sx=s.sxb=2.0f;s.sy=s.syb=2.0f;s.sz=2.0f;s.zn=1.0f;s.shp_ct=-100.0f;
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,5.0f,10.0f,15.0f);
    float lm[16];std::copy(m,m+16,lm);s.link_matrix=lm;
    check(step_effect_sprite(s,c,&queued)&&queued,"bhEff352 first pass did not queue");
    check(s.tex_id==8&&s.bl_src==8&&s.bl_dst==10&&(s.flg&0x4180000u)==0x4180000u,"bhEff352 init differs");
    check(close(s.xn,2.0f)&&close(s.yn,0.25f),"bhEff352 growth constants differ");
    check(s.mode0==2&&s.mode2==1,"bhEff352 did not enter mode 2");
    check(close(s.sxb,0.25f),"bhEff352 did not grow by yn");
    // ofs (0,0,0), owk T(5,10,15), cam.v (0,0,-1), zn 1: p2 (5,10,15), p1 -cam*zn+p2-owk_trans
    // = (0,0,1); die Drehlage von inv(owk) ist die Identitaet, also lox/loy/loz = (0,0,1),
    // Zeichenursprung owk*local = (5,10,16) -> op->mtx[13] = 10.
    check(close(s.lox,0.0f)&&close(s.loy,0.0f)&&close(s.loz,1.0f),"bhEff352 local position differs");
    check(close(s.mtx_world_y,10.0f),"bhEff352 world Y differs");
    check(close(s.sx,0.25f*0.48f),"bhEff352 UV size differs");
    // Bodenabschluss (bhEff352: shp_ct > op->mtx[13]).
    s.shp_ct=100.0f;
    step_effect_sprite(s,c,&queued);
    check(s.flg==0,"bhEff352 did not end when the ground is above the sprite");
}
// Produktiver Linkzeichenpass: bhEff352 schreibt lox/loy/loz pro Kamera,
// effect.c:1346..1349 zeichnet genau diesen Versatz. Nicht das Erzeuger-offset.
// Testaufstellung: an den bereits veroeffentlichten Spielerbaum gebundene Flamme
// (derselbe generische Effektlink wie fuer EN01), keine neue Motionpose.
static void flame352_scene(const std::filesystem::path& data){
    const auto room=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0040");
    auto player=PlayerAsset::load(data,10);world.set_player_asset(&player);
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    world.tick();std::vector<BoneMatrix> nodes;(void)world.sample_player(player,&nodes);nodes=player.script_nodes(nodes);world.commit_player_joints(nodes);
    EffectSpawn spawn;spawn.id=352;spawn.type=0;spawn.scale={2,2,0};spawn.lkono=9;
    spawn.has_offset=true;spawn.offset={0.1f,0.2f,0.3f};spawn.has_velocity=true;spawn.velocity={0,0,1.5f};
    const int slot=world.spawn_player_effect(spawn);check(slot>=0,"linked flame allocation failed");
    for(int view=0;view<2;++view){
        auto camera=stage.cameras.at(0);camera.angles={view==0?0:4096,view==0?0:16384,0};
        world.tick_camera(stage,camera,0);world.tick();
        const auto* s=world.effect_state(unsigned(slot));check(s&&world.effect_visible(unsigned(slot)),"linked flame not queued");
        // cam.vx des Effektpasses: bhControlCamera des Vorbildes (game.c:50 vor :58), danach laeuft sie fuer dieses Bild.
        const auto context=world.effect_context();NJS_POINT3 expected;njCalcPoint(&nodes.at(9).value,&spawn.offset,&expected);
        expected.x-=context.camera_view.x*spawn.velocity.z;expected.y-=context.camera_view.y*spawn.velocity.z;expected.z-=context.camera_view.z*spawn.velocity.z;
        Scene draw;world.append(draw,nodes,camera);bool found=false;
        for(std::size_t i=0;i+1<draw.triangles.size();++i){const auto& a=draw.triangles[i];const auto& b=draw.triangles[i+1];
            if(a.color!=s->tv[0].col||a.blend!=SceneBlend::Ninja||!a.textured||a.texture!=b.texture)continue;
            // Lokale Eck-Y -1.5 / +0.5: Ursprung = Mittelpunkt + sy * Billboard-Y.
            NJS_MATRIX billboard;njUnitMatrix(&billboard);njRotateY(&billboard,-camera.angles[1]);njRotateX(&billboard,-camera.angles[0]);njRotateZ(&billboard,-camera.angles[2]);njScale(&billboard,1,-1,-1);
            NJS_POINT3 halfheight{0,0.5f*s->sy,0};njCalcVector(&billboard,&halfheight,&halfheight);
            const auto p=a.vertices[0].position,q=b.vertices[1].position;
            const NJS_POINT3 origin{(p.x+q.x)/2+halfheight.x,(p.y+q.y)/2+halfheight.y,(p.z+q.z)/2+halfheight.z};
            if(close(origin.x,expected.x)&&close(origin.y,expected.y)&&close(origin.z,expected.z)){found=true;break;}
        }
        check(found,"draw ignored bhEff352's camera-adjusted lox/loy/loz");
        world.control_camera(stage,camera); // game.c:58
    }
    {
        // effect.c:812 bhControlEffect: vor der Prozedur px = owP[lkono] * (lox,loy,loz) des Satzes,
        // also der von bhEff352 im vorigen Pass geschriebene Kameraversatz, nicht der Erzeuger-Versatz
        // aus bhSetEffectTb (effect.c:481). Dritte Kameraansicht: lox aendert sich, px traegt den alten Wert.
        const auto* s=world.effect_state(unsigned(slot));const NJS_POINT3 previous{s->lox,s->loy,s->loz};
        auto camera=stage.cameras.at(0);camera.angles={-4096,-16384,0};world.tick_camera(stage,camera,0);world.tick();s=world.effect_state(unsigned(slot));
        check(s&&!(close(s->lox,previous.x)&&close(s->loy,previous.y)&&close(s->loz,previous.z)),"test setup: third view did not move bhEff352's lox/loy/loz");
        NJS_POINT3 expected,stale;njCalcPoint(&nodes.at(9).value,const_cast<NJS_POINT3*>(&previous),&expected);njCalcPoint(&nodes.at(9).value,&spawn.offset,&stale);
        check(close(s->px,expected.x)&&close(s->py,expected.y)&&close(s->pz,expected.z),"bhControlEffect px does not follow owP * (lox,loy,loz) of the previous pass");
        check(!(close(s->px,stale.x)&&close(s->py,stale.y)&&close(s->pz,stale.z)),"bhControlEffect px still uses the producer offset instead of op->lox");
    }
    // flg 0x200000 erzwingt freie Weltlage, auch wenn der Satz einen Link hat.
    EffectSpawn free;free.id=122;free.type=1;free.flg=0x200001u;free.position={40,50,60};free.scale={1,1,1};free.has_offset=true;free.offset={900,800,700};
    const int unlinked=world.spawn_player_effect(free);check(unlinked>=0,"free effect allocation failed");world.tick();
    // bhControlEffect:812 aktualisiert px auch bei 0x200000; der Zeichenpass
    // benutzt dann diesen Weltort statt ein zweites Mal lox zu transformieren.
    const auto* free_work=world.effect_state(unsigned(unlinked));const NJS_POINT3 free_position{free_work->px,free_work->py,free_work->pz};
    Scene draw;world.append(draw,nodes,stage.cameras.at(0));bool found=false;
    for(std::size_t i=0;i+1<draw.triangles.size();++i){const auto& a=draw.triangles[i];const auto& b=draw.triangles[i+1];
        if(a.blend!=SceneBlend::Ninja||!a.textured||a.texture!=b.texture)continue;
        const auto p=a.vertices[0].position,q=b.vertices[1].position;
        if(close((p.x+q.x)/2,free_position.x)&&close((p.y+q.y)/2,free_position.y)&&close((p.z+q.z)/2,free_position.z))found=true;
    }
    check(found,"0x200000 world-space effect consumed its ignored link offset");
}

// Effektaufrufe der eff[]-Zeichenpaesse laufen nicht durch njClipZ (SceneTriangle::ninja_primitive):
// njDrawTextureSub3D (ps2_NaDraw.c:302) verwirft den Aufruf bei scr.z < 0, Ps2AddPrim3D
// (ps2_dummy.c:486) laesst ein Streifendreieck fallen, wenn ein Eckpunkt ausserhalb +-2047
// Bildpunkte um 2048 oder ausserhalb 1 <= z <= 3854 liegt. Eine njClipZ-Nahebene von 9 (Kamera
// der RM_0020-Explosion) darf einen Effekt bei z = 5 nicht abschneiden.
static void effect_call_clip(){
    SceneCamera camera;camera.perspective=14016;camera.clip_near=9.0f;camera.clip_far=0;
    // Sicht z = -Welt z, Sicht y = -Welt y (camera_view_matrix: njScale(1,-1,-1)).
    const auto quad=[](std::array<NJS_POINT3,4> view,std::uint32_t id,bool screen_reject){
        std::array<SceneVertex,4> v;for(unsigned i=0;i<4;++i)v[i]=SceneVertex{{view[i].x,-view[i].y,-view[i].z}};
        std::vector<SceneTriangle> out;
        for(const auto& index:std::array<std::array<unsigned,3>,2>{{{0,1,2},{1,3,2}}}){
            SceneTriangle t;t.vertices={v[index[0]],v[index[1]],v[index[2]]};t.color=0xffffffffu;t.world_lit=true;t.camera_mask=false;
            t.ninja_primitive=id;t.ninja_screen_reject=screen_reject;out.push_back(t);}
        return out;};
    const auto pixels=[&](const std::vector<SceneTriangle>& triangles){
        Scene scene;scene.background=0xff000000;scene.triangles=triangles;
        SoftwareRenderer image(64,48);image.set_supersampling(1);image.draw(scene,camera);
        return std::count_if(image.pixels().begin(),image.pixels().end(),[](auto p){return p!=0xff000000u;});};
    const std::array<NJS_POINT3,4> near_quad{{{-1,-1,5},{1,-1,5},{-1,1,5},{1,1,5}}};
    check(pixels(quad(near_quad,0,true))==0,"test setup: an ordinary triangle at z 5 is not clipped by njClipZ 9");
    const auto both=pixels(quad(near_quad,1,true));
    check(both>0,"effect call at z 5 still clipped by njClipZ near 9");
    // Halbe Breite 30 bei z = 5: x*ninja_sx/z liegt weit jenseits 2047 -> beide Dreiecke ADC.
    check(pixels(quad({{{-30,-1,5},{30,-1,5},{-30,1,5},{30,1,5}}},1,true))==0,"effect triangle beyond the GS +-2047 window still drawn");
    // Nur v3 jenseits: Dreieck (v0,v1,v2) bleibt, (v1,v2,v3) faellt.
    const auto half=pixels(quad({{{-1,-1,5},{1,-1,5},{-1,1,5},{30,1,5}}},1,true));
    check(half>0&&half<both,"Ps2AddPrim3D drops whole calls instead of the flagged strip triangle");
    // z 0.5 an v3 (|1.0625z-2048.06| > 2047): dasselbe Dreieck faellt, keine Nahklippung.
    check(pixels(quad({{{-1,-1,5},{1,-1,5},{-1,1,5},{1,1,0.5f}}},1,true))==half,"vertex closer than z 1 not flagged by the Ps2AddPrim3D z test");
    // scr.z < 0: njDrawTextureSub3D verwirft den ganzen Aufruf, njDrawPolygonSub3D (ef_ntx) nicht.
    const std::array<NJS_POINT3,4> behind{{{-1,-1,5},{1,-1,5},{-1,1,5},{1,1,-1}}};
    check(pixels(quad(behind,1,true))==0,"njDrawTextureSub3D call with scr.z < 0 still drawn");
    check(pixels(quad(behind,1,false))==half,"njDrawPolygonSub3D lost the triangle without the scr.z < 0 vertex");
}

// bhDrawWeaponEffect (effsub1.c:1220) zeichnet jedes Viereck mit njDrawTexture3DEx und damit ueber
// njDrawTextureSub3D (ps2_NaDraw.c:302): ein Eckpunkt mit scr.z < 0 beendet den ganzen Aufruf (:327).
// RM_3000 (Steve, Waffe 8, Ereignis 12): Steve schiesst in der Nahaufnahme auf die Kamera zu; die
// Flare-Speichen (Typ 2) reichen hinter die Kamera. Eine Nahebenen-Klippung zeichnete sie als
// bildbreite Streifen vor dem Gesicht statt sie wie das Original ganz zu verwerfen.
static void weapon_flash_call_clip(){
    SceneCamera camera;camera.perspective=14016;camera.clip_near=2.0f;camera.clip_far=0;
    NJS_MATRIX view;camera_view_matrix(camera,view);
    // Muendung 2.2 vor der Kamera (njClipZ near 2), Lauf (+x der Gelenkmatrix) zeigt auf die Kamera.
    WeaponFlash f;f.flg=0x1001;f.type=2;f.mdlver=0;f.flr_no=3;f.sx=2;
    const auto toward=[&](Angle ay){NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,ay);NJS_POINT3 x{1,0,0},w;njCalcVector(&m,&x,&w);NJS_POINT3 v;njCalcVector(&view,&w,&v);return v.z;};
    const Angle ay=toward(16384)<toward(-16384)?Angle(16384):Angle(-16384); // Sicht-z faellt = zur Kamera
    {NJS_POINT3 at{0.8f,0.6f,0},front{0,0,1}; /* seitlich versetzt: die Flaechen liegen nicht durch das Auge */NJS_POINT3 v;njCalcVector(&view,&front,&v); // Welt-z mit Sicht-z > 0
     const float s=v.z>0?2.2f:-2.2f;njUnitMatrix(&f.matrix);njTranslate(&f.matrix,at.x,at.y,s);njRotateY(&f.matrix,ay);}
    f.tick();Scene scene;f.append(scene,0);
    check(!scene.triangles.empty(),"weapon flash fixture drew nothing");
    std::vector<SceneTriangle> behind,ordinary;
    for(std::size_t i=0;i<scene.triangles.size();){
        const auto id=scene.triangles[i].ninja_primitive;
        check(id!=0&&scene.triangles[i].ninja_screen_reject,"bhDrawWeaponEffect quad without the njDrawTextureSub3D call rule");
        std::size_t j=i;bool back=false;
        for(;j<scene.triangles.size()&&scene.triangles[j].ninja_primitive==id;++j)
            for(const auto& vertex:scene.triangles[j].vertices){NJS_POINT3 p=vertex.position,q;njCalcPoint(&view,&p,&q);back=back||q.z<0;}
        if(back)for(std::size_t k=i;k<j;++k){behind.push_back(scene.triangles[k]);auto o=scene.triangles[k];o.ninja_primitive=0;o.ninja_screen_reject=false;ordinary.push_back(o);}
        i=j;}
    check(!behind.empty(),"test setup: no flare spoke reaches behind the camera");
    const auto pixels=[&](const std::vector<SceneTriangle>& triangles){
        Scene s;s.background=0xff000000;s.triangles=triangles;
        for(unsigned t=0;t<4;++t)s.textures.push_back(SceneTexture{1,1,TexturePixels(std::vector<std::uint32_t>(1,0xffffffffu))});
        SoftwareRenderer image(64,48);image.set_supersampling(1);image.draw(s,camera);
        return std::count_if(image.pixels().begin(),image.pixels().end(),[](auto p){return p!=0xff000000u;});};
    check(pixels(ordinary)>0,"test setup: near-clipped spokes would not reach the image");
    check(pixels(behind)==0,"weapon-flash spoke with scr.z < 0 still drawn (njDrawTextureSub3D drops the call)");
}

// effsub1.c:5197-5204 bhEff023: mdlver 0 in sys->ef_pnc, sonst sys->ef_trs. effect.c:886 zeichnet ef_pnc
// unter njControl3D(0x4000); Ps2AddPrim3D loescht dann ABE (ps2_dummy.c:510-516): kein Mischen,
// Alphatest GREATER 0 (:543), sofort mit Tiefe (Ps2ZbuffOn :2011) statt ueber die OT.
static void explosion_punch_through(){
    EffectContext c;
    for(unsigned mdlver:{0u,1u}){
        auto s=work(23,0,{1,1,1});s.mdlver=mdlver;bool queued=false;
        check(step_effect_sprite(s,c,&queued)&&queued,"bhEff023 did not enter a draw list");
        check(s.punch_queued==(mdlver==0),"bhEff023 mdlver 0 must go to sys->ef_pnc, others to sys->ef_trs");
        SceneCamera camera;camera.perspective=8192;Scene scene;
        append_effect_sprite(scene,s,{0,0,-40.0f},{1,1,1},camera,0);
        check(scene.triangles.size()==2,"bhEff023 quad not drawn");
        for(const auto& t:scene.triangles){
            if(mdlver==0)check(t.punch_through&&t.blend==SceneBlend::Opaque&&t.depth_write,"ef_pnc draws with the ALPHA register or without depth instead of GS without ABE");
            else check(!t.punch_through&&t.blend==SceneBlend::Ninja&&!t.depth_write,"ef_trs lost its 8/10 blend");
        }
    }
    // GS ohne ABE: ein Texel mit Alpha > 0 schreibt seine Farbe ganz, Alpha 0 faellt am Test durch.
    Scene scene;scene.background=0xff202020;{std::vector<std::uint32_t> px(16,0x00ffffffu);std::fill(px.begin(),px.begin()+4,0x80c0c0c0u);std::fill(px.begin()+4,px.begin()+8,0x10c0c0c0u); /* Zeile 0 nur fuer die hoechste Alpha: kein Teilabdeckungsmass */scene.textures={SceneTexture{4,4,TexturePixels(px)}};}
    SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2},0,0},SceneVertex{{4,-4,2},1,0},SceneVertex{{4,4,2},1,1}};
    a.color=0xffffffff;a.world_lit=true;a.textured=true;a.blend=SceneBlend::Opaque;a.punch_through=true;a.texture_filter=SceneTextureFilter::Point;
    auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2},0,1}};scene.triangles={a,b};
    SoftwareRenderer renderer(32,32);renderer.set_supersampling(1);NJS_MATRIX view;njUnitMatrix(&view);
    for(bool filter:{false,true}){
        renderer.set_texture_filtering(filter);scene.triangles[0].texture_filter=scene.triangles[1].texture_filter=filter?SceneTextureFilter::Linear:SceneTextureFilter::Point;
        renderer.draw_view(scene,view,16,16);
        check(renderer.pixels()[2*32+16]==0xffc0c0c0u,"punch-through texel with alpha 0x10 is blended instead of written whole");
        check(renderer.pixels()[30*32+16]==0xff202020u,"punch-through texel with alpha 0 is not rejected by the alpha test");
        check(renderer.covered_pixels()>0,"punch-through does not write depth");
        // Eine spaeter eingereihte, weiter hinten liegende Flaeche darf nur durch die Alpha-0-
        // Loecher sichtbar sein. Das prueft die Tiefe am selben Pixel, nicht nur irgendeinen Eintrag.
        auto back=a;for(auto& v:back.vertices){v.position.x*=2;v.position.y*=2;v.position.z=4;}
        back.textured=false;back.punch_through=false;back.color=0xff2030a0u;
        auto back2=back;back2.vertices={back.vertices[0],back.vertices[2],SceneVertex{{-8,8,4}}};
        scene.triangles.push_back(back);scene.triangles.push_back(back2);
        renderer.draw_view(scene,view,16,16);
        check(renderer.pixels()[2*32+16]==0xffc0c0c0u&&renderer.depth()[2*32+16]==.5f,"later geometry overwrites punch-through depth");
        check(renderer.pixels()[30*32+16]==0xff2030a0u&&renderer.depth()[30*32+16]==.25f,"alpha-0 hole blocks later geometry");
        scene.triangles.resize(2);
    }
}

// Diagnoseaufstellung im echten Raumbesitzer, kein natuerliches Durchspielen: eff023 aus dem
// gemeinsamen eff[512]-Pool muss mit originalem SYSTEM-Atlas durch RoomWorld::append gelangen.
static void explosion_punch_world(const std::filesystem::path& data){
    const auto bytes=read_asset_file(data/"rooms/RM_0080.RDT",64*1024*1024);const auto stage=decode_scene(bytes);
    auto actor=stage.spawns.at(0);std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(bytes,stage,actor,motion,frame,audio,data,{},"RM_0080");
    world.clear_effects();world.set_simulation_flags(8u);
    SceneCamera camera;camera.perspective=8192;
    EffectSpawn spawn;spawn.id=23;spawn.flg=1;spawn.type=0;spawn.mdlver=0;spawn.scale={8,8,8};spawn.position={0,0,-40};
    const int slot=world.spawn_unlinked_effect(spawn);check(slot>=0,"bhEff023 cannot use the existing world pool");
    const auto* atlas=world.effect_group_textures(9);check(atlas&&!atlas->empty(),"bhEff023 original SYSTEM atlas 9 missing");
    unsigned visible=0;
    for(unsigned pass=0;pass<16;++pass){
        world.tick();Scene draw;world.append(draw,{},camera);
        std::vector<SceneTriangle> punch;for(const auto& t:draw.triangles)if(t.punch_through)punch.push_back(t);
        if(pass<15){
            check(world.effect_visible(unsigned(slot))&&punch.size()==2,"ef_pnc work does not reach productive world drawing");
            for(const auto& t:punch)check(t.textured&&t.texture<draw.textures.size()&&!draw.textures[t.texture].pixels.empty()&&t.depth_write&&t.blend==SceneBlend::Opaque&&t.ninja_primitive&&t.ninja_screen_reject,"world punch-through lost texture, depth, immediate pass or call clip rule");
            Scene only;only.background=0xff202020;only.textures=draw.textures;only.triangles=std::move(punch);
            SoftwareRenderer image(128,128);image.set_supersampling(1);image.draw(only,camera);
            visible+=unsigned(std::count_if(image.pixels().begin(),image.pixels().end(),[](auto p){return p!=0xff202020u;})>0);
        }else check(!world.effect_visible(unsigned(slot))&&punch.empty(),"terminated ef_pnc work still draws");
    }
    check(visible>0,"original SYSTEM atlas produces no punch-through pixels");
    world.clear_effects();Scene cleared;world.append(cleared,{},camera);
    check(std::none_of(cleared.triangles.begin(),cleared.triangles.end(),[](const auto& t){return t.punch_through;}),"clear_effects retains ef_pnc geometry");
}

static void punch_filter_rgb(){
    // TFX MODULATE vor ATST (ps2_dummy.c:1615-1625/1653), ABE aus: Texel-RGB wird NICHT mit
    // At premultipliziert. Die gruenen Texel mit At 0 tragen zur bilinearen RGB-Mischung bei,
    // erst die separat gefilterte Alpha entscheidet ueber Farbe UND Tiefe.
    {
        Scene s;s.background=0xff202020;SceneTexture t;t.width=2;t.height=1;
        t.pixels=TexturePixels(std::vector<std::uint32_t>{0xff0000ffu,0x0000ff00u});t.clamp_u=t.clamp_v=true;s.textures={t};
        SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2},.5f,.5f},SceneVertex{{4,-4,2},.5f,.5f},SceneVertex{{4,4,2},.5f,.5f}};
        a.color=0xffffffff;a.world_lit=true;a.textured=true;a.blend=SceneBlend::Opaque;a.punch_through=true;a.texture_filter=SceneTextureFilter::Linear;
        auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2},.5f,.5f}};s.triangles={a,b};
        SoftwareRenderer r(32,32);r.set_supersampling(1);r.set_render_threads(1);NJS_MATRIX view;njUnitMatrix(&view);
        r.draw_view(s,view,16,16);
        check(r.pixels()[16*32+16]==0xff008080u,"punch-through RGB filtering is alpha-weighted: half opaque blue/transparent green must be cyan, not blue");
        check(r.depth()[16*32+16]==.5f,"punch-through filtered alpha>0 must write full depth");
        // Derselbe Filterbesitzer fuer die optionale Host-Minifikation: ein 4x4-Schachbrett
        // muss auch im Mip cyan bleiben; ein nachtraegliches Entmischen repariert premultiplizierte Mips nicht.
        std::vector<std::uint32_t> checker(16);for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)checker[y*4+x]=(x+y)&1?0x0000ff00u:0xff0000ffu;
        s.textures[0].width=s.textures[0].height=4;s.textures[0].pixels=TexturePixels(checker);s.textures[0].invalidate_mips();
        for(auto& tr:s.triangles){tr.texture_filter=SceneTextureFilter::Default;for(auto& v:tr.vertices){v.u=(v.position.x+4)*8;v.v=(v.position.y+4)*8;}}
        r.set_texture_filtering(true);r.draw_view(s,view,16,16);
        check(r.pixels()[16*32+16]==0xff008080u,"punch-through mip RGB was averaged with alpha weighting");
    }

}

// Lazy Punch-Mips: Aufbau vor Bandthreads, Fingerprint-/Detach-Vertrag der SceneTexture.
static void punch_lazy_cache(){
Scene s;s.background=0xff202020;SceneTexture t;t.width=t.height=4;t.clamp_u=t.clamp_v=true;std::vector<unsigned> px(16);for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)px[y*4+x]=(x+y)&1?0x0000ff00:0xff0000ff;t.pixels=TexturePixels(px);s.textures={t};
SceneTriangle a;a.vertices={SceneVertex{{-4,-4,2},0,0},SceneVertex{{4,-4,2},64,0},SceneVertex{{4,4,2},64,64}};a.color=0xffffffff;a.world_lit=true;a.textured=true;auto b=a;b.vertices={a.vertices[0],a.vertices[2],SceneVertex{{-4,4,2},0,64}};s.triangles={a,b};SoftwareRenderer r(32,32);r.set_supersampling(1);r.set_texture_filtering(true);r.set_render_threads(1);NJS_MATRIX m;njUnitMatrix(&m);
auto draw=[&]{r.draw_view(s,m,16,16);};
for(auto filter:{SceneTextureFilter::Linear,SceneTextureFilter::Default}){for(auto&tr:s.triangles)tr.texture_filter=filter;draw();check(s.textures[0].filter_cache->punch_mips.empty(),"normal filtered texture eagerly created punch mips");}
for(auto&tr:s.triangles){tr.punch_through=true;tr.texture_filter=SceneTextureFilter::Linear;}draw();check(s.textures[0].filter_cache->punch_mips.empty(),"Punch Linear must not create unused mips");
for(auto&tr:s.triangles)tr.texture_filter=SceneTextureFilter::Default;draw();auto shared=s.textures[0].filter_cache;check(!shared->punch_mips.empty(),"Punch Default failed to create mips");check(r.pixels()[16*32+16]==0xff008080,"Punch mip colour not cyan");
// Unchanged texture copies reuse the cache. Pixel edit must detach both pixels and cache;
// another original owner remains valid and unchanged.
SceneTexture other=s.textures[0];draw();check(s.textures[0].filter_cache==shared,"unchanged Punch rebuilt the cache");
s.textures[0].pixels[0]=0xffffffff;draw();check(s.textures[0].filter_cache!=shared,"pixel revision did not detach shared cache");check(!s.textures[0].filter_cache->punch_mips.empty(),"detached Punch lost mip chain");check(s.textures[0].filter_cache->punch_mips.back()!=shared->punch_mips.back(),"pixel mutation reused stale Punch mip");check(other.filter_cache==shared&&other.filter_cache->punch_mips.back()[0]==0x7f008080,"mutated copy corrupted original cache");
s.textures[0].pixels.assign(16,0xffff0000);for(auto&tr:s.triangles)tr.punch_through=false;draw();check(s.textures[0].filter_cache->punch_mips.empty(),"ordinary draw after revision retained stale punch cache");for(auto&tr:s.triangles)tr.punch_through=true;draw();check(r.pixels()[16*32+16]==0xffff0000,"new Punch reused old pixel data");
s.textures[0].invalidate_mips();check(s.textures[0].filter_cache->punch_mips.empty(),"explicit invalidation retained Punch chain");draw();check(r.pixels()[16*32+16]==0xffff0000,"explicit invalidation changed colour");
}

// effsub4.c:1432/2237, effect.c:37 registration. Diagnostic work inputs,
// fixed original table cells/clocks; not a claim of natural combat progression.
static void head_burst_effects(){
    EffectContext c;bool queued=false;std::vector<CollisionWall> walls;
    set_effect_walls(&walls,0);set_effect_pool_lookup({});take_effect_output();
    for(unsigned id:{362u,370u,371u,372u,373u})check(supported_effect_sprite(id),"head blood effect dispatch missing");
    for(unsigned seed=1;seed<9;++seed){
        recvx_core_random_seed(seed);const int delay=recvx_core_random_next()%3;
        const float speed=2.0f+effect_detail::effect_random();const auto after=recvx_core_random_next();
        recvx_core_random_seed(seed);auto s=work(362,4,{2,3,0});s.xn=1;s.yn=2;s.zn=3;s.tex_id=7;
        auto& r=effect_detail::request_template();r.ax=123;r.ay=456;r.flr_no=-3;
        check(step_effect_sprite(s,c,&queued),"362 dispatch failed");
        check(recvx_core_random_next()==after,"362 RNG order/count differs: delay before speed");
        check(s.tex_id==7&&s.bl_src==8&&s.bl_dst==3&&(s.stflg&0x20)&&s.tv[0].col==0xff808080u,"362 texture/blend/color differs");
        check(close(s.spd,delay?speed:speed*.97f)&&queued==(delay==0),"362 init fallthrough or random speed differs");
        for(int k=0;k<delay;++k)step_effect_sprite(s,c,&queued);
        check(queued&&s.mode0==2&&s.ct2==1&&close(s.px,speed*.97f),"362 delay/motion clock differs");
        check(close(s.tv[0].u,0)&&close(s.tv[0].v,224.f/255)&&close(s.tv[3].u,31.f/255)&&close(s.sx,.64f),"362 type4 atlas UV/size differs");
        auto out=take_effect_output();check(out.gaps.empty()&&out.spawns.size()==1,"362 first blood5 spawn count differs");
        const auto& child=out.spawns[0];check(child.id==372&&child.mdlver==1&&child.scale.x==3&&child.scale.z==0&&child.ay==0&&child.ax==123&&child.flr_no==-3,"362 blood5 template writes/inheritance differ");
        check(child.has_velocity&&child.velocity.x==-1&&child.velocity.y==0&&child.velocity.z==-3&&child.has_shp_ct&&child.shp_ct==.5f&&child.ao.x==.98f&&child.ao.y==-.03f,"362 blood5 child fields differ");
        for(int k=1;k<151;++k){s.pxb=s.px;s.pyb=s.py;s.pzb=s.pz;step_effect_sprite(s,c,&queued);}
        check(s.mode0==3&&s.ct2==151,"362 150-step ballistic lifetime differs");
        for(int k=0;k<6;++k){step_effect_sprite(s,c,&queued);check(s.flg&&queued,"362 fade released early");}
        step_effect_sprite(s,c,&queued);check(!s.flg&&!queued,"362 seventh fade pass (alpha 0x1f) must release");
        take_effect_output();
    }
    // 370 types select linked vs detached 371/373; type9 uses original particles.
    NJS_MATRIX matrix;njUnitMatrix(&matrix);njRotateY(&matrix,16384);matrix[12]=10;matrix[13]=20;matrix[14]=30;
    const unsigned ids[11]={371,371,371,371,371,371,373,373,373,0,373};
    const int lm[11]={0,0,0,1,0,1,1,1,0,2,2},wait[11]={4,4,4,4,2,3,4,3,2,3,3};
    for(unsigned type=0;type<11;++type){
        EffectWorldHooks hooks;int particles=0;
        hooks.particle=[&](int kind,unsigned index,int node,NJS_POINT3 off,NJS_POINT3 dir,unsigned color,int typ){
            check(kind==1&&index==2&&node==12&&off.x==1&&dir.x==1&&color==0xff400000u&&typ==3,"370 type9 particle binding differs");++particles;return 42;};
        set_effect_world_hooks(std::move(hooks));
        auto s=work(370,type,{3.5f,3.5f,0});s.flg|=0x80;s.link_kind=1;s.link_index=2;s.lkono=12;s.link_matrix=matrix;
        s.lox=1;s.loy=2;s.loz=3;s.xn=1;s.yn=2;s.zn=3;s.tex_id=8;s.slot=22;s.ct0=2;s.ct1=3;
        auto& r=effect_detail::request_template();r.ax=12;r.ay=34;r.flr_no=-2;
        recvx_core_random_seed(123);const auto initial=recvx_core_random_next();recvx_core_random_seed(123);
        for(int k=0;k<2;++k){step_effect_sprite(s,c,&queued);check(!queued&&take_effect_output().spawns.empty(),"370 initial delay drew/spawned");}
        step_effect_sprite(s,c,&queued);auto out=take_effect_output();
        check(!queued&&out.gaps.empty()&&s.ct2==1&&s.ct0==wait[type],"370 emitter state differs");
        if(type==9){check(particles==1&&out.spawns.empty()&&recvx_core_random_next()==initial,"370 particle path consumed RNG/spawned sprite");}
        else{
            check(out.spawns.size()==1,"370 must emit one child per interval");const auto& child=out.spawns[0];
            check(child.id==ids[type]&&child.ax==12&&child.ay==34&&child.flr_no==-2&&child.has_shp_ct&&child.has_ao,"370 template/child fields differ");
            check(child.mdlver==(type==8?3u:(type==6||type==7)?1u:0u)&&child.type==0&&child.scale.x==3.5f,"370 color/type/scale table differs");
            check(child.link_kind==(lm[type]==0?1:-1)&&child.has_offset==(lm[type]==0),"370 link mode differs");
            NJS_POINT3 v{1,2,3},p{1,2,3};njCalcVector(&matrix,&v,&v);njCalcPoint(&matrix,&p,&p);
            check(close(child.velocity.x,lm[type]==1?v.x:1)&&close(child.velocity.z,lm[type]==1?v.z:3),"370 direction transform differs");
            if(lm[type])check(close(child.position.x,p.x)&&close(child.position.y,p.y),"370 detached origin differs");
            else check(child.position.x==0&&child.offset.x==1&&child.lkono==12,"370 retained joint offset differs");
            if(child.id==371)check(child.texture_from_slot==22&&child.tex_id_override==8&&(child.stflg_or&0x20),"370 model texture handoff differs");
        }
        int ticks=0;while(s.flg&&ticks++<30){step_effect_sprite(s,c,&queued);take_effect_output();}
        check(!s.flg&&ticks==2*(wait[type]+1),"370 spawn count/interval/lifetime differs");
    }
    set_effect_world_hooks({});
    // Full eff[512]: 370 still updates template and count, but never consumes rand.
    EffectSprite occupied;occupied.flg=2;set_effect_pool_lookup([&](int){return &occupied;});
    auto full=work(370,0);full.ct1=1;recvx_core_random_seed(14);const auto rng=recvx_core_random_next();recvx_core_random_seed(14);
    step_effect_sprite(full,c,&queued);check(!full.flg&&take_effect_output().spawns.empty()&&recvx_core_random_next()==rng,"370 full pool changed RNG or count");set_effect_pool_lookup({});
    // Child init is blank; first animation is next pass, then original terminator.
    for(unsigned id:{371u,372u,373u}){
        EffectSpawn e;e.id=id;e.mdlver=1;e.scale={2,3,0};e.has_velocity=true;e.velocity={1,2,3};e.has_shp_ct=true;e.shp_ct=.5f;e.has_ao=true;e.ao={.98f,-.03f,0};
        EffectSprite s;initialize_effect_sprite(s,e);check(s.shp_ct==.5f&&s.aoy==-.03f,"blood child spawn fields lost");
        if(id==371){s.tex_id=8;s.flg|=0x80;s.lox=10;s.px=100;}
        step_effect_sprite(s,c,&queued);check(!queued&&s.mode0==1,"blood child init must not draw");
        step_effect_sprite(s,c,&queued);check(queued&&s.mode0==2&&s.ct1==1,"blood child first active clock differs");
        check(s.tex_id==(id==371?8:5)&&s.ani_ct==(id==371?0:1)&&s.bl_src==8&&s.bl_dst==3,"blood child texture/blend differs");
        if(id==371)check(close(s.lox,10.49f)&&s.px==100,"371 must move local offset, not world origin");
        int active=1;while(s.flg&&active<25){step_effect_sprite(s,c,&queued);++active;}
        check(!s.flg&&!queued&&active==(id==371?15:id==372?9:11),"blood child finite animation lifetime differs");
    }
    // ef_trs (bhEff_SetSprite mode 0) with 8/3: ALPHA register blend, no depth write.
    {auto s=work(373,0);bool q=false;s.ct3=0;step_effect_sprite(s,c,&q);step_effect_sprite(s,c,&q);check(q&&!s.custom_queued&&!s.punch_queued&&!s.opaque_queued,"373 must use sys->ef_trs");
     SceneCamera camera;camera.perspective=8192;Scene scene;append_effect_sprite(scene,s,{0,0,-40.0f},{1,1,1},camera,0);
     check(scene.triangles.size()==2,"blood ef_trs quad absent");
     for(const auto& t:scene.triangles)check(!t.punch_through&&t.blend==SceneBlend::Ninja&&!t.depth_write,"blood translucency/depth differs");}
    set_effect_walls(nullptr,0);take_effect_output();
}


// effect.c:949 bhDrawPolEffect ueber sys->ef_pol: je Satz objP[op->mdlver].model von
// sys->efm[op->id] (njCnkEasyDrawModel, :1021). Die Textur ist
// sys->ef_tlist.textures[op->tex_id + op->ani_ct] DIREKT, ohne sys->ef_tn — belegt an
// ELF 0x0021D3F8..0x0021D424 (nur die Summe der Felder), waehrend bhDrawTrsEffect3D an
// 0x0021DEBC dieselben Felder mit sys->ef_tn[tex_id] + ani_ct liest. Huelse (bhEff000,
// tex_id 5), Magazin (bhEff007, tex_id 7) und Markierung (bhEff013, tex_id 8) liegen damit
// alle in den neun Bildern von Kategorie 0.
static void pol_effect_draw(const std::filesystem::path& data){
    const auto system=read_asset_file(data/"archives/SYSTEM/0001.bin",8*1024*1024);
    const auto at=common_effect_textures_offset(system);
    check(at!=std::size_t(-1),"SYSTEM 0001: gemeinsame Effekttexturen nicht gefunden");
    const auto common=decode_texture_bundle(system,at);
    check(common.size()>9,"SYSTEM 0001: weniger als neun gemeinsame Effekttexturen");
    const auto room=read_asset_file(data/"rooms/RM_0040.RDT",64*1024*1024);const auto stage=decode_scene(room);
    auto actor=stage.spawns.at(0);std::size_t motion=0;std::int64_t frame=0;AudioMixer audio(data);
    RoomWorld world(room,stage,actor,motion,frame,audio,data,{},"RM_0040");
    Inventory inventory;MessageDialog dialog;EventVm events(room,inventory,dialog);world.bind_player_collision(events);
    world.tick();
    const NJS_POINT3 at_pos{actor.position.x,actor.position.y+6.0f,actor.position.z};
    // Derselbe Weg wie bhSetGunFire/bhSetMagazine: die bhSetEffectTb-Anforderung des Kampfkerns.
    auto svc=world.effect_combat_services();
    const auto place=[&](unsigned id,unsigned mdlver,unsigned type,int flr_no){
        RECVX_CB_EFFECT req{};req.ef.flg=1;req.ef.id=std::uint16_t(id);req.ef.type=std::uint16_t(type);
        req.ef.mdlver=std::uint16_t(mdlver);req.ef.flr_no=std::int16_t(flr_no);
        req.ef.px=at_pos.x;req.ef.py=at_pos.y;req.ef.pz=at_pos.z;req.ef.sx=req.ef.sy=req.ef.sz=1.0f;
        (void)svc.effect(svc.user,&req);};   // bhSetEffectTb reiht in die Passliste ein
    // bhEff000 stirbt sofort, wenn flr_no == sys->yk_ct (effsub1.c:259); yk_ct ist hier 0.
    place(0,0,1,1);place(0,1,1,1);place(7,0,0,0);
    world.tick();
    const EffectSprite *a=nullptr,*b=nullptr,*m=nullptr;
    for(unsigned i=0;const auto* s2=world.effect_state(i);++i){
        if(!(s2->flg&1u))continue;
        if(s2->id==0&&s2->mdlver==0&&!a)a=s2;
        else if(s2->id==0&&s2->mdlver==1&&!b)b=s2;
        else if(s2->id==7&&!m)m=s2;}
    check(a&&b&&m&&a->polygon_queued&&b->polygon_queued&&m->polygon_queued,"ef_pol: die drei Saetze stehen nicht in der Liste");
    check(a->tex_id==5&&m->tex_id==7,"ef_pol: tex_id 5 (Huelse) / 7 (Magazin)");
    const auto draw=[&]{Scene out;world.append(out,{},stage.cameras.at(0));return out;};
    const Scene with=draw();
    for(const auto* s:{a,b,m})const_cast<EffectSprite*>(s)->stflg|=0x1000000u;   // effect.c:965 verdeckt den Aufruf
    const Scene without=draw();
    for(const auto* s:{a,b,m})const_cast<EffectSprite*>(s)->stflg&=~0x1000000u;
    check(with.triangles.size()>without.triangles.size(),"ef_pol: die Modelle fehlen im Bild");
    // Die zusaetzlichen Texturen sind genau die gemeinsamen Bilder 5 (zweimal) und 7.
    const auto same=[](const SceneTexture& x,const SceneTexture& y){
        return x.width==y.width&&x.height==y.height&&x.pixels.size()==y.pixels.size()&&
               (x.pixels.empty()||std::equal(x.pixels.begin(),x.pixels.end(),y.pixels.begin()));};
    const auto count=[&](const Scene& sc,const SceneTexture& t){unsigned n=0;for(const auto& u:sc.textures)if(same(u,t))++n;return n;};
    check(count(with,common[5])==count(without,common[5])+2,"bhDrawPolEffect: Huelse nicht mit dem gemeinsamen Bild 5");
    check(count(with,common[7])==count(without,common[7])+1,"bhDrawPolEffect: Magazin nicht mit dem gemeinsamen Bild 7");
    // objP[mdlver]: die beiden Huelsenobjekte von efm[0] sind verschiedene Teilmengen,
    // zusammen das ganze Modell (zwei Objekte, 40 Dreiecke).
    const auto added=[&](const EffectSprite* keep){
        for(const auto* s:{a,b,m})if(s!=keep)const_cast<EffectSprite*>(s)->stflg|=0x1000000u;
        const Scene one=draw();
        for(const auto* s:{a,b,m})const_cast<EffectSprite*>(s)->stflg&=~0x1000000u;
        return one.triangles.size()-without.triangles.size();};
    const auto n0=added(a),n1=added(b),nm=added(m);
    check(n0>0&&n1>0&&n0+n1==40,"bhDrawPolEffect: objP[mdlver] waehlt nicht das eine Objekt (efm[0] hat zwei, zusammen 40 Dreiecke)");
    check(nm>0,"bhDrawPolEffect: das Magazinmodell efm[7] zeichnet nichts");
    for(const auto& g:world.unbound_enemy_routines())check(g.find("bhDrawPolEffect")==std::string::npos,"bhDrawPolEffect meldet eine Luecke");
}
// effsub2.c, zweiter Satz (bhJumpEffect2, effect.c:35): 251 DropBlood, 266 FloorBlood2, 268 E14_Explosion,
// 271 E14_Fire2, 272 Dust, 273 E14_Mucus, 299 DamagePoint. Die Erwartungen sind aus den Originalzeilen
// nachgerechnet; Zufallswerte werden vorab ueber frac() aus derselben Folge gezogen.
namespace recvx {void set_effect_work_node_matrix(std::function<bool(const void* work,unsigned node,NJS_MATRIX& out)> lookup);}
static void effsub2_second_batch(){
    install_group4_services();
    for(const unsigned id:{251u,266u,268u,271u,272u,273u,299u})check(supported_effect_sprite(id),"effsub2 zweiter Satz nicht als Handport registriert");
    EffectContext c;c.pt_flg=0x10;bool queued=false;take_effect_output();
    std::vector<CollisionWall> walls{{1,0,0,0,0,{-50,-10,-50},{100,10,100}}};GroundCollision ground;ground.update(walls,100.f);set_effect_ground(&ground,nullptr,nullptr);
    check(close(ground.query({0,2,0}),0.0f)&&close(ground.query({0,20,0}),0.0f),"Testboden liegt nicht bei y 0");
    { // 251 :290 — Fall ab y 2: drei Fallbilder, Landung auf yn = 0.2, Aufprall ab ct1 3 (kein Ruecksetzen).
        EffectSpawn e;e.id=251;e.flg=1;e.position={0,2,0};e.scale={1.5f,1.5f,1.5f};EffectSprite s;initialize_effect_sprite(s,e);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==5&&s.bl_src==8&&s.bl_dst==3&&s.flg==(1u|0x180000u)&&s.tv[3].col==0xFFFFFFFFu,"251 :319-:331 Start");
        check(close(s.yn,0.2f)&&close(s.py,2.0f-0.71f)&&close(s.ofy,-0.86f)&&s.ct1==1,"251 :335/:349-:357 Boden + 0.2, erster Fallschritt");
        check(close(s.tv[0].u,0.84375f)&&close(s.tv[0].v,0.15625f)&&close(s.tv[3].u,0.90625f)&&close(s.tv[3].v,0.21875f)&&close(s.sx,1.5f),"251 BH_UVTAB0[0], bhEff_SetUVInfo Basis 0.0625");
        step_effect_sprite(s,c,&queued);check(queued&&s.mode0==1&&close(s.py,0.43f)&&close(s.tv[0].u,0.90625f),"251 zweiter Fallschritt");
        step_effect_sprite(s,c,&queued);check(queued&&s.mode0==2&&close(s.py,0.2f)&&s.ct1==3&&close(s.tv[0].u,0.53125f),"251 :359 Landung auf yn, Bild noch aus BH_UVTAB0");
        step_effect_sprite(s,c,&queued);check(queued&&s.ct1==4&&close(s.tv[0].u,0.9375f)&&close(s.tv[0].v,0.3125f),"251 :367 Aufprall beginnt bei BH_UVTAB1[ct1 = 3]");
        step_effect_sprite(s,c,&queued);check(queued&&close(s.tv[0].v,0.375f),"251 BH_UVTAB1[4]");
        step_effect_sprite(s,c,&queued);check(!queued&&s.flg==0,"251 :369 Endzeile beendet den Satz");
    }
    { // 251 langer Fall: Tabellenende -> ct1 = 1 (:345-:348), ofy sinkt je Pass um 0.15 bis unter -2.
        EffectSpawn e;e.id=251;e.flg=1;e.position={0,20,0};EffectSprite s;initialize_effect_sprite(s,e);
        for(int i=0;i<5;++i)step_effect_sprite(s,c,&queued);
        check(s.ct1==5&&s.mode0==1,"251 fuenf Fallbilder");
        step_effect_sprite(s,c,&queued);
        check(queued&&s.ct1==2&&close(s.tv[0].u,0.90625f)&&close(s.ofy,-0.71f-6*0.15f),"251 :345-:348 Endzeile -> Zeile 1, ct1 2");
        for(int i=0;i<6;++i)step_effect_sprite(s,c,&queued);
        check(s.mode0==1&&close(s.ofy,-2.06f),"251 :352 ofy bleibt nach Unterschreiten von -2 stehen");
    }
    { // 272 :971 — Staub, Farbe aus ct0 vor dem Abzug, 22 Bilder.
        EffectSpawn e;e.id=272;e.flg=1;e.position={0,5,0};EffectSprite s;initialize_effect_sprite(s,e);s.xn=1.0f;
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==2&&s.bl_src==8&&s.bl_dst==10&&s.flg==(1u|0x180000u),"272 :1010-:1018 Start");
        check(s.tv[0].y==-2&&s.tv[1].y==-2&&s.tv[2].y==0&&s.tv[3].x==1&&s.tv[0].z==0,"272 bhEff_SetAlign(op, 2)");
        check(s.tv[0].col==0x32373737u&&s.tv[3].col==0x32373737u&&s.ct0==48&&s.ct1==1,"272 :1029-:1031 Farbe aus ct0, danach ct0 -= 2");
        check(close(s.tv[0].u,0.25f)&&close(s.sx,0.125f/0.09375f)&&close(s.px,1.0f)&&close(s.xn,0.8f),"272 Zeile 0, Basis 0.09375, Lage += Richtung, Richtung * 0.8");
        unsigned passes=1;
        while(s.flg&&passes<40){step_effect_sprite(s,c,&queued);++passes;if(passes==2)check(s.tv[0].col==0x30373737u&&close(s.px,1.8f)&&close(s.tv[0].u,0.5f),"272 zweiter Pass");}
        check(passes==23&&!queued&&s.tv[0].col==0x06373737u,"272 22 Bilder, im 23. Pass Endzeile (Farbe noch gesetzt)");
    }
    { // 268 :3881 — drei Saetze 19 ueber sys->ef; flr_no stammt vom letzten Schreiber.
        effect_detail::request_template().flr_no=7;
        EffectSpawn e;e.id=268;e.flg=1;e.position={1,2,3};EffectSprite s;initialize_effect_sprite(s,e);
        EffectContext v=c;v.camera_view={0.5f,0.0f,-1.0f};
        take_effect_output();step_effect_sprite(s,v,&queued);auto out=take_effect_output();
        check(!queued&&out.spawns.size()==2,"268 :3885-:3926 zwei Saetze 19");
        const auto a=out.spawns[0],b=out.spawns[1];
        check(a.id==19&&a.flg==0x4100001u&&a.type==0&&a.mdlver==0&&close(a.scale.x,2.5f)&&close(a.scale.z,2.5f)&&close(a.position.x,1)&&close(a.position.y,2)&&a.ax==0&&a.ay==0&&a.flr_no==7&&a.lkono==0,"268 erster Satz type 0 Groesse 2.5");
        check(b.id==19&&b.flg==0x4100001u&&b.type==2&&close(b.scale.y,5.0f)&&close(b.position.z,3),"268 zweiter Satz type 2 Groesse 5");
        step_effect_sprite(s,v,&queued);step_effect_sprite(s,v,&queued);
        check(s.flg&&s.ct0==2&&take_effect_output().spawns.empty(),"268 :3933 ct0 1 und 2 ohne Ausgabe");
        step_effect_sprite(s,v,&queued);out=take_effect_output();
        check(s.flg==0&&out.spawns.size()==1&&out.spawns[0].type==1&&close(out.spawns[0].scale.x,2.8f)&&close(out.spawns[0].position.x,1.5f)&&close(out.spawns[0].position.y,2.0f)&&close(out.spawns[0].position.z,2.0f),"268 :3935-:3954 dritter Satz an px + cam.vx/vy/vz, dann frei");
    }
    { // 271 :4214 — Flamme am Spielergelenk 3.
        recvx_core_random_seed(31);const float r0=frac(),r1=frac();recvx_core_random_seed(31);
        EffectSpawn e;e.id=271;e.flg=1;EffectSprite s;initialize_effect_sprite(s,e);s.stflg=0x20u;
        step_effect_sprite(s,c,&queued);
        check(queued&&(s.flg&0x4180081u)==0x4180081u&&s.tex_id==9&&s.bl_src==8&&s.bl_dst==10&&!(s.stflg&0x20u)&&s.tv[2].col==0xFFFFFFFFu,"271 :4242-:4251 Start");
        check(close(s.lox,1.0f-2.0f*r0)&&close(s.loy,-0.5f+2.0f*r1)&&close(s.loz,-1.5f),"271 :4253-:4264 Versatz, Zufallsreihenfolge lox vor loy");
        check(s.link_kind==0&&s.lkono==3&&s.lkwkp==nullptr,"271 :4266-:4267 lkwkp = plp, lkono 3");
        check(close(s.sxb,2.0f)&&s.ct0==0xFF&&s.frm_no==1&&close(s.sx,4.0f)&&close(s.tv[0].u,0.5f)&&close(s.tv[0].v,0.5f),"271 BH_UVTAB0[0], Basis 0.125");
        step_effect_sprite(s,c,&queued);
        check(close(s.sxb,1.83f)&&close(s.sx,3.66f)&&close(s.tv[0].u,0.25f)&&s.frm_no==2,"271 :4284 sxb += 0.1 (0.3 - sxb) vor dem Tabellenschritt");
        unsigned passes=2;while(s.flg&&passes<40){step_effect_sprite(s,c,&queued);++passes;}
        check(passes==12&&!queued,"271 elf Bilder, Ruecksprung auf 0 beendet im zwoelften Pass");
        EffectSprite t;initialize_effect_sprite(t,e);t.mode1=1;step_effect_sprite(t,c,&queued);check(close(t.loz,1.3f),"271 mode1 != 0 -> loz 1.3");
    }
    { // 266 :3092 — Wartezeit, ein Spielertreffer, dann Flamme am Gelenk 3.
        struct Pl {RECVX_ENEMY_PLAYER p{};int fires=0;NJS_POINT3 first{};float size=0;int life=0;} pl;
        EffectWorldHooks h;
        h.player_get=[&pl](RECVX_ENEMY_PLAYER& out){out=pl.p;return true;};
        h.player_set=[&pl](const RECVX_ENEMY_PLAYER& in){pl.p=in;};
        h.player_fire=[&pl](int jno,NJS_POINT3 o,float size,int life){if(!pl.fires){pl.first=o;pl.size=size;pl.life=life;}check(jno==3,"266: bhEne_SetFireEffect(plp, 3, ...)");++pl.fires;};
        set_effect_world_hooks(h);
        pl.p.hp=100;
        recvx_core_random_seed(41);float r[19];for(auto& x:r)x=frac();recvx_core_random_seed(41);
        EffectSpawn e;e.id=266;e.flg=1;EffectSprite s;initialize_effect_sprite(s,e);s.mode1=1;
        const int wait=int(10.0f*r[0])+40;
        step_effect_sprite(s,c,&queued);
        check(!queued&&s.mode0==1&&s.ct0==wait-1,"266 :3118 ct0 = 40 + 10r, der erste Pass zaehlt schon");
        int passes=1;while(s.mode0==1&&passes<100){step_effect_sprite(s,c,&queued);++passes;check(!queued,"266 wartet ohne Bild");}
        check(passes==wait+1&&s.mode0==2,"266 :3123 `ct0-- == 0` nach ct0 + 1 Passes");
        check(pl.p.hp==80&&(pl.p.flg&0x10004u)==0x10004u&&(pl.p.stflg&0x10000u)&&pl.p.mode0==4&&pl.p.mode2==1&&pl.p.mode3==0,"266 :3127-:3134 Spielertreffer, mode2 = op->mode1");
        check(pl.fires==4&&close(pl.first.x,1.0f-2.0f*r[1])&&close(pl.first.y,-0.5f+2.0f*r[2])&&close(pl.first.z,1.3f)&&close(pl.size,0.5f+r[3])&&pl.life==int(10.0f*r[4])+20,"266 :3136-:3158 vier Feuer, Zufallsreihenfolge x, y, Groesse, Dauer");
        step_effect_sprite(s,c,&queued);
        check(queued&&s.tex_id==9&&!(s.stflg&0x20u)&&(s.flg&0x4180081u)==0x4180081u&&s.bl_dst==10&&s.tv[0].col==0xFFFFFFFFu,"266 :3165-:3176 Flamme");
        check(close(s.lox,1.0f-2.0f*r[17])&&close(s.loy,-0.5f+2.0f*r[18])&&close(s.loz,1.3f)&&s.link_kind==0&&s.lkono==3,"266 :3178-:3191 Versatz am Spielergelenk 3");
        check(close(s.sxb,2.5f)&&close(s.sx,5.0f)&&s.frm_no==1,"266 sxb 2.5, BH_UVTAB0[0] Basis 0.125");
        passes=0;while(s.flg&&passes<40){step_effect_sprite(s,c,&queued);++passes;}
        check(passes==8&&!queued,"266 acht Bilder, Ruecksprung auf 0 beendet");
        pl.p=RECVX_ENEMY_PLAYER{};pl.p.hp=50;pl.p.flg=4u;pl.fires=0;
        EffectSprite t;initialize_effect_sprite(t,e);t.mode0=1;t.ct0=0;
        step_effect_sprite(t,c,&queued);
        check(t.mode0==2&&pl.p.hp==50&&pl.fires==0,"266 :3125 plp->flg 0x4 sperrt den Treffer, mode0 laeuft weiter");
        set_effect_world_hooks({});
    }
    { // 273 :4311 / :4494 — Schleimfaden zwischen Raumpunkt und Gelenk der dritten Alexia-Form.
        RECVX_ENEMY_WORK ene[3]{};ene[0].flg=1;ene[0].id=13;ene[1].flg=0;ene[1].id=14;ene[2].flg=1;ene[2].id=14;
        EffectProjectileServices svc;svc.enemy=[&ene](int i)->RECVX_ENEMY_WORK*{return i>=0&&i<3?&ene[i]:nullptr;};set_effect_projectile_services(svc);
        set_effect_work_node_matrix([&ene](const void* w,unsigned node,NJS_MATRIX& out){if(w!=&ene[2]||node!=11)return false;njUnitMatrix(&out);njTranslate(&out,10,20,30);njRotateY(&out,16384);return true;});
        EffectSpawn e;e.id=273;e.flg=1;e.position={90,33,91};e.scale={0.5f,-0.25f,2.0f};e.ay=8192;e.ax=16384;
        // :4449-:4470 mit sx/sy des Vorpasses: owP[lkono] T(sx, sy, 0) RotZ(ax) (+-sz, 0, 0).
        const auto joint_end=[](float sx,float sy,float x){NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,10,20,30);njRotateY(&m,16384);njTranslate(&m,sx,sy,0);njRotateZ(&m,16384);NJS_POINT3 v{x,0,0},o;njCalcPoint(&m,&v,&o);return o;};
        NJS_MATRIX ry;njUnitMatrix(&ry);njRotateY(&ry,8192);NJS_POINT3 d{2,0,0};njCalcVector(&ry,&d,&d);
        EffectSprite s;initialize_effect_sprite(s,e);s.lkono=11;
        step_effect_sprite(s,c,&queued);
        check(s.lkwkp==&ene[2],"273 :4391-:4398 erstes ene[] mit flg & 1 und id 14");
        check(s.tex_id==81&&s.ani_ct==1&&s.bl_src==8&&s.bl_dst==10&&(s.flg&0x24080001u)==0x24080001u&&s.mode0==1,"273 :4376-:4389 Start");
        check(close(s.tv[2].x,90+d.x)&&close(s.tv[2].z,91+d.z)&&close(s.tv[3].x,90-d.x)&&close(s.tv[3].y,33),"273 :4432-:4446 Weltende px +- RotY(ay) (sz,0,0)");
        const NJS_POINT3 a=joint_end(0.5f,-0.25f,2),b=joint_end(0.5f,-0.25f,-2);
        check(close(s.tv[0].x,a.x)&&close(s.tv[0].y,a.y)&&close(s.tv[0].z,a.z)&&close(s.tv[1].x,b.x)&&close(s.tv[1].z,b.z),"273 :4449-:4470 Gelenkende mit sx/sy aus dem Satz");
        check(close(s.sx,0.5f)&&close(s.sy,-0.375f)&&close(s.tv[0].u,0)&&close(s.tv[3].v,0.1875f),"273 :4472 bhEff_SetUVInfo erst nach den Ecken (sy = syb * 1.5)");
        check(queued&&s.custom_queued&&s.quads.size()==1&&s.quads[0].world_space&&s.quads[0].category==81&&s.quads[0].frame==1&&s.quads[0].source==8&&s.quads[0].destination==10&&!s.quads[0].point_filter&&s.quads[0].fogged&&s.fnc_exit_src==8&&s.fnc_exit_dst==6,"273 :4494-:4517 MucusDraw: Weltecken, Gruppe 81 Bild 1, 8/10, bilinear, danach 8/6");
        check(close(s.quads[0].vertices[0].x,a.x)&&close(s.quads[0].vertices[2].x,90+d.x),"273 Viereck aus tv");
        step_effect_sprite(s,c,&queued);
        const NJS_POINT3 a2=joint_end(0.5f,-0.375f,2);
        check(close(s.tv[0].x,a2.x)&&close(s.tv[0].y,a2.y)&&close(s.tv[0].z,a2.z)&&s.mode0==1,"273 zweiter Pass rechnet mit dem sy aus bhEff_SetUVInfo");
        s.mode1=2;step_effect_sprite(s,c,&queued);check(s.mode0==2&&close(s.tv[0].v,0)&&s.frm_no==0,"273 :4406 mode1 schaltet um, Bild noch BH_UVTAB0[0]");
        unsigned passes=0;
        while(s.mode0==2&&passes<40){step_effect_sprite(s,c,&queued);++passes;if(passes==1)check(close(s.tv[0].v,0.1875f)&&s.frm_no==1,"273 :4414 uvtble[mode1 - 1] = BH_UVTAB1");}
        check(passes==12&&s.custom_queued,"273 BH_UVTAB1: zwoelf Bilder, dann mode0 3");
        step_effect_sprite(s,c,&queued);check(!queued&&!s.custom_queued&&s.flg,"273 :4424 mode0 3 zeichnet nicht und bleibt belegt");
        EffectSprite hidden;initialize_effect_sprite(hidden,e);hidden.lkono=11;hidden.flg|=0x1000000u;step_effect_sprite(hidden,c,&queued);
        check(hidden.custom_queued&&hidden.quads.empty()&&hidden.fnc_exit_src==-1&&hidden.fnc_exit_dst==-1,"273 :4496 flg 0x1000000: eingereiht, kein Zeichenaufruf, Blendzustand unberuehrt");
        ene[2].flg=0;EffectSprite none;initialize_effect_sprite(none,e);none.lkono=11;step_effect_sprite(none,c,&queued);
        check(!none.lkwkp&&!queued&&none.mode0==1,"273 :4427 ohne aktiven Gegner 14 kein Faden");
        ene[2].flg=1;
        EffectSprite joint;initialize_effect_sprite(joint,e);joint.lkono=5;take_effect_output();step_effect_sprite(joint,c,&queued);
        check(!queued&&!take_effect_output().gaps.empty()&&close(joint.sy,-0.375f),"273 Gelenk ohne Besitzer: benannte Luecke, Zeilenschritt laeuft weiter");
        EffectSprite bad;initialize_effect_sprite(bad,e);bad.lkono=11;bad.mode1=4;step_effect_sprite(bad,c,&queued);take_effect_output();step_effect_sprite(bad,c,&queued);
        check(bad.flg==0&&!take_effect_output().gaps.empty(),"273 mode1 - 1 ausserhalb uvtble[3] ist eine benannte Luecke");
        set_effect_work_node_matrix({});set_effect_projectile_services({});
    }
    { // 299 :4520 — springender Punkt ohne Zeichenaufruf.
        EffectSpawn e;e.id=299;e.flg=1;e.position={0,3,0};EffectSprite s;initialize_effect_sprite(s,e);
        step_effect_sprite(s,c,&queued);
        check(queued&&s.custom_queued&&s.quads.empty()&&s.fnc_exit_src==-1&&s.fnc_exit_dst==-1&&s.ct0==5&&close(s.ofy,0.6f)&&close(s.yn,0.2f)&&close(s.py,3.0f),"299 :4524-:4535 Start ohne Bewegung, ef_fnc ohne Viereck");
        unsigned passes=1,first_bounce=0;float bounce_py=0,bounce_ofy=0;
        while(s.flg&&passes<200){const unsigned m=s.mode0;const int ct=s.ct0;step_effect_sprite(s,c,&queued);++passes;if(!first_bounce&&m==1&&s.ct0!=ct){first_bounce=passes;bounce_py=s.py;bounce_ofy=s.ofy;}}
        check(first_bounce==11&&close(bounce_py,0.32f)&&close(bounce_ofy,0.56f),"299 :4544-:4552 erster Bodenkontakt: ofy *= -0.4, py = yn + 0.6 (yn - py)");
        check(passes==41,"299 sechs Bodenkontakte, dann sechs Passes Nachlauf");
    }
    set_effect_ground(nullptr,nullptr,nullptr);take_effect_output();
}
// effsub2.c:1336 bhEff_E02_SandParticle2 (255): Wartezeit 120 + 30r, danach mit r < 0.3 ein Korn 254,
// dessen sx/sy/sz (3) und sxb/syb/szb (0.2/0.3/0.2) getrennt nachgetragen werden.
static void sand_particle2(){
    check(supported_effect_sprite(255),"255 nicht als Handport registriert");
    EffectContext c;bool queued=false;take_effect_output();
    effect_detail::request_template().scale={7,7,7};
    recvx_core_random_seed(51);float r[40];for(auto& x:r)x=frac();recvx_core_random_seed(51);
    EffectSpawn e;e.id=255;e.flg=1;e.position={4,5,6};EffectSprite s;initialize_effect_sprite(s,e);
    int k=0;bool spawned=false;
    for(int cycle=0;cycle<20&&!spawned;++cycle){
        step_effect_sprite(s,c,&queued);
        const int wait=int(120.0f+30.0f*r[k++]);
        check(!queued&&s.mode0==1&&s.ct0==wait,"255 :1343 ct0 = 120 + 30r");
        for(int i=0;i<wait;++i){step_effect_sprite(s,c,&queued);check(take_effect_output().spawns.empty(),"255 wartet ohne Ausgabe");}
        step_effect_sprite(s,c,&queued);
        const float roll=r[k++];const auto out=take_effect_output();
        check(!queued&&s.mode0==0,"255 :1351 nach Ablauf mode0 = 0, zeichnet nicht");
        if(roll<0.3f){
            check(out.spawns.size()==1,"255 :1353 r < 0.3: genau ein 254");
            const auto& g=out.spawns[0];
            check(g.id==254&&g.flg==1&&g.type==0&&g.ax==0&&g.ay==0&&close(g.position.x,4)&&close(g.position.y,5)&&close(g.position.z,6),"255 :1355-:1367 sys->ef des Korns");
            check(g.has_scale_base&&close(g.scale.x,3)&&close(g.scale.z,3)&&close(g.scale_base.x,0.2f)&&close(g.scale_base.y,0.3f)&&close(g.scale_base.z,0.2f),"255 :1371-:1379 eff[eno].sx = 3, sxb/syb/szb 0.2/0.3/0.2");
            EffectSprite grain;initialize_effect_sprite(grain,g);
            check(close(grain.sx,3)&&close(grain.sy,3)&&close(grain.sxb,0.2f)&&close(grain.syb,0.3f)&&close(grain.szb,0.2f),"255: der neue Satz traegt sx und sxb getrennt");
            check(close(effect_detail::request_template().scale.x,7),"255 schreibt sys->ef.sx nicht");
            spawned=true;
        }else check(out.spawns.empty(),"255 r >= 0.3: kein Korn");
    }
    check(spawned,"255: in 20 Zyklen kein Korn (Zufallsfolge pruefen)");
}
int main(int argc,char** argv){
    try{
        _Make_SinTable();
        cartridge_case();sign_plate();blinking_lamp();fire();panel();smoke();puff002();
        muzzle_weapon229();flame352();effect_call_clip();weapon_flash_call_clip();explosion_punch_through();punch_filter_rgb();punch_lazy_cache();
        effsub0_producers();effsub0_sprites();effsub0_batch_b();effsub1_batch();group2_producers();group2_sprites();group3_services();group4_custom_draw();group5_rest();group6_h_vertices();group7_backface_and_eyes();group8_gas_wall();group9_snow();group10_afterimages();acid_generator();afterimage_color_contract();head_burst_effects();effsub2_second_batch();sand_particle2();
        if(argc>1){const std::filesystem::path data=argv[1];explosion_punch_world(data);steam(data);power_switch_positions(data);flame352_scene(data);production(data);muzzle_flash(data);eye_lights(data);gas_room(data);snow_room(data);ntx2d_room(data);afterimage_room(data);published_afterimage_models(data);pol_effect_draw(data);}
        std::cout<<"effect room sprites ok\n";return 0;
    }catch(const std::exception& error){std::cerr<<"effect room sprites failed: "<<error.what()<<"\n";return 1;}
}
