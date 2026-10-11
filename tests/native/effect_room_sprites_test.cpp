// Portierte Raum-Effektprozeduren: bhEff122/123/125/126/181/182/218/230.
// Geprueft werden Zustandsautomat, Originaldaten und die tatsaechliche
// Zeichenausgabe auf dem produktiven RoomWorld-Besitzer mit Originalraeumen.
#include "world.h"
#include "asset_io.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "core_random.h"
#include "chunk_blend.h"
#include "weapon_flash.h"
#include "weapon_tables.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
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

int main(int argc,char** argv){
    try{
        _Make_SinTable();
        sign_plate();blinking_lamp();fire();panel();smoke();puff002();
        if(argc>1){const std::filesystem::path data=argv[1];steam(data);production(data);muzzle_flash(data);}
        std::cout<<"effect room sprites ok\n";return 0;
    }catch(const std::exception& error){std::cerr<<"effect room sprites failed: "<<error.what()<<"\n";return 1;}
}
