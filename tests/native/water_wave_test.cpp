// bhObj005 (objitm.c:1213) und bhEff108 (effsub1.c:6435) in den vier Originalraeumen mit
// Wasserflaeche (RM_3060, RM_3210, RM_7060, RM_9130): echte RoomWorld-Dienste, die 109-
// Raumerzeuger des Raums stellen die 108-Ringe. Semantikreferenz world_water.cpp (Kopf).
#include "world.h"
#include "effect_sprites_internal.h"
#include "room.h"
#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    explicit Fixture(const std::filesystem::path& data,const char* room):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    void gaps(const char* phase){
        // Belegte Original-UB: RM_3210 hat Eckpunkte bei x/z = 0, deren Nachbarzellen vor wt_wvp
        // liegen (objitm.c:1419-1431 liest den Heapblock davor); benannt, nicht ersetzt.
        for(const auto& g:world.unbound_enemy_routines())if((g.find("bhObj005")!=std::string::npos||g.find("bhEff108")!=std::string::npos)&&g.find("Nachbarzelle ausserhalb wt_wvp")==std::string::npos)
            throw std::runtime_error(std::string(phase)+": Luecke "+g);
    }
};
static void ring_unit(){
    // effsub1.c:6435 bhEff108 ohne Raum: Karte 8x8 an (10, 20), lkono 13 -> sy 1.3, car 1, spd 0.65.
    auto& wt=water_wave();wt=WaterWaveField{};wt.px=10;wt.pz=20;wt.xp=8;wt.zp=8;wt.maxx=8;wt.maxz=8;wt.nbpt=64;wt.wvp.assign(64,0.0f);wt.allocated=true;
    EffectContext c;c.st_flg=0x40u;c.gm_flg=0;
    EffectSprite s;s.id=108;s.flg=1;s.lkono=13;s.px=13.7f;s.py=0;s.pz=24.2f;
    bool queued=false;step_effect_sprite(s,c,&queued);
    check(s.mode0==1&&std::fabs(s.px-3.7f)<1e-5f&&std::fabs(s.pz-4.2f)<1e-5f,"bhEff108: px/pz minus wt_px/wt_pz");
    check(std::fabs(s.sy-1.29f)<1e-5f&&std::fabs(s.car-1.65f)<1e-5f&&std::fabs(s.spd-0.642f)<1e-5f,"bhEff108: sy 1.3-0.01, car 1+0.65, spd 0.65-0.008");
    // Bresenham r=1 um (3,4): nur (3,5),(3,3),(4,4),(2,4) auf 1.3.
    int raised=0;for(int x=0;x<8;++x)for(int z=0;z<8;++z){const float h=wt.wvp[std::size_t(z+x*8)];if(h!=0.0f){++raised;check(std::fabs(h-1.3f)<1e-6f,"bhEff108: Ringhoehe sy");
        check((x==3&&(z==5||z==3))||(z==4&&(x==4||x==2)),"bhEff108: Ringzellen r=1");}}
    check(raised==4,"bhEff108: vier Ringzellen");
    // Bis sy < 0 (130 Passes) bleibt das Werk, danach flg 0.
    int passes=1;while(s.flg&&passes<200){step_effect_sprite(s,c,&queued);++passes;}
    check(!s.flg&&passes==131,"bhEff108: frei nach 131 Passes (sy -0.01 je Pass)");
    // Zellen ausserhalb (negativ/>= max) fallen heraus: Ring um (-1,-1) schreibt nur in [0,max).
    wt.wvp.assign(64,0.0f);EffectSprite t;t.id=108;t.flg=1;t.lkono=5;t.px=wt.px-1.0f+0.5f;t.pz=wt.pz-1.0f+0.5f;
    step_effect_sprite(t,c,&queued);// ox = (int)(-0.5) = 0 (C-Trunkierung)
    for(int x=0;x<8;++x)for(int z=0;z<8;++z){const float h=wt.wvp[std::size_t(z+x*8)];if(h!=0.0f)check((x==0&&z==1)||(x==1&&z==0),"bhEff108: Rand (unsigned short)");}
    // Ohne st_flg 0x40 oder mit gm_flg 1: flg 0.
    EffectContext off;off.st_flg=0;EffectSprite u;u.id=108;u.flg=1;step_effect_sprite(u,off,&queued);check(u.flg==0,"bhEff108: ohne st_flg 0x40 frei");
    wt=WaterWaveField{};
}
// hitchk.c:5561 bhCheckL2Water und pwksub.c:2643 bhSetWaterSplash4 auf der echten Flaechenliste
// des Raums (rom->flrp vor sys->mflrp, type 3, prm0 1), dieselbe wie bhCheckWater.
static void l2_water_room(Fixture& f,const char* room){
    const auto& floors=f.events.floor_areas();const Interaction* a=nullptr;
    for(const auto& x:floors)if((x.flags&1)&&x.type==3&&x.parameters[0]==1){a=&x;break;}
    check(a!=nullptr,"bhCheckL2Water: Raum ohne Wasserflaeche type 3 / prm0 1");
    const float top=a->position.y+a->extent.y,cx=a->position.x+a->extent.x*0.5f,cz=a->position.z+a->extent.z*0.5f;
    std::cout<<room<<": Wasserflaeche x "<<a->position.x<<" +"<<a->extent.x<<", z "<<a->position.z<<" +"<<a->extent.z<<", Oberflaeche "<<top<<"\n";
    NJS_POINT3 hit{};
    check(f.world.l2_water({cx,top+10.0f,cz},{0,-20.0f,0},hit),"bhCheckL2Water: senkrechte Linie trifft die Oberflaeche");
    check(std::fabs(hit.y-top)<1e-4f&&hit.x==cx&&hit.z==cz,"bhCheckL2Water: Schnittpunkt py + h unter dem Ursprung");
    float probe_top=0;check(f.world.water_top({cx,top-0.01f,cz},probe_top)&&probe_top==top,"bhCheckWater sieht dieselbe Flaeche");
    check(!f.world.l2_water({cx,top+10.0f,cz},{0,-9.9f,0},hit),"bhCheckL2Water: |p - pt| > |v| ist kein Treffer");
    check(!f.world.l2_water({cx,top+10.0f,cz},{0,20.0f,0},hit),"bhCheckL2Water: Oberflaeche hinter dem Ursprung (InnerProduct <= 0)");
    check(!f.world.l2_water({cx,top+10.0f,cz},{20.0f,0,0},hit),"bhCheckL2Water: parallele Linie (njDistanceL2PL != 0)");
    check(!f.world.l2_water({-100000.0f,top+10.0f,-100000.0f},{0,-20.0f,0},hit),"bhCheckL2Water: ausserhalb w/d");
    check(f.world.l2_water({cx-10.0f,top+10.0f,cz},{20.0f,-20.0f,0},hit)&&std::fabs(hit.x-cx)<1e-3f&&std::fabs(hit.y-top)<1e-3f&&std::fabs(hit.z-cz)<1e-3f,
          "bhCheckL2Water: schraege Linie schneidet bei (cx, top, cz)");
    // bhSetWaterSplash4: zwei id-11-Saetze type+10, ay +/- 16384, lkono 0/1, ax aus sys->ef.
    std::vector<bool> before(512,false);for(unsigned i=0;i<512;++i){const auto* e=f.world.effect_state(i);before[i]=e&&(e->flg&1u);}
    effect_detail::request_template().ax=1234;
    f.world.water_splash4({cx,top,cz},1000,1,1.6f,1.6f,1.6f);
    std::vector<const EffectSprite*> made;for(unsigned i=0;i<512;++i){const auto* e=f.world.effect_state(i);if(e&&(e->flg&1u)&&!before[i])made.push_back(e);}
    check(made.size()==2,"bhSetWaterSplash4: zwei neue Saetze");
    for(unsigned k=0;k<2;++k){const auto& e=*made[k];
        check(e.id==11&&e.type==11&&e.lkono==int(k)&&e.mdlver==0&&e.flr_no==0,"bhSetWaterSplash4: id 11, type 1+10, lkono 0 dann 1");
        check(((e.ay-(k?1000-16384:1000+16384))&0xffff)==0&&(e.ax&0xffff)==1234,"bhSetWaterSplash4: ay = ang +/- 16384, ax aus der Vorlage");
        check(e.px==cx&&e.py==top&&e.pz==cz&&e.sx==1.6f&&e.sy==1.6f&&e.sz==1.6f,"bhSetWaterSplash4: Lage und Skalierung");}
    effect_detail::request_template().ax=0;
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    ring_unit();
    for(const char* room:{"RM_3060","RM_3210","RM_7060","RM_9130"}){
        water_wave()=WaterWaveField{}; // neuer Raum: bhGetFreeMemory-Arena
        Fixture f(data,room);
        // Genau ein Typ-5-Objekt (bhJumpObject[5]).
        int water=-1,count=0;for(unsigned i=0;i<256;++i){const auto* e=f.world.state(2,i);if(!e)continue;if((e->type&0xffu)==5){water=int(i);++count;}} // bhSetObject: type = (unsigned char)ETTY type
        std::cout<<room<<": Wasserobjekt "<<water<<" ("<<count<<" Typ-5-Objekte)\n";
        if(count!=1)for(unsigned i=0;i<256;++i){const auto* e=f.world.state(2,i);if(!e)continue;std::cout<<"  obj "<<i<<": id "<<e->identity<<" type "<<e->type<<" flags 0x"<<std::hex<<e->object_flags<<std::dec<<" aspd "<<e->aspd<<" draw_tp "<<e->draw_tp<<"\n";}
        check(count==1,"ein Wasserobjekt je Raum");
        l2_water_room(f,room);
        check(!(f.events.flags[5][0]&0x40u),"st_flg 0x40 vor dem ersten Durchlauf frei");
        f.tick();
        const auto* w=f.world.water_work(unsigned(water));check(w&&w->mode0==1&&!w->orphan,"bhObj005 Init: mode0 1");
        f.gaps("Init");
        const auto& wt=water_wave();const auto* e=f.world.state(2,unsigned(water));
        float minx=1e9f,minz=1e9f,maxx=-1e9f,maxz=-1e9f;for(const auto& p:w->base){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);minz=std::min(minz,p.z);maxz=std::max(maxz,p.z);}
        std::cout<<"  chunk "<<w->chunk_type<<", "<<w->base.size()<<" Eckpunkte, x "<<minx<<".."<<maxx<<", z "<<minz<<".."<<maxz<<", Lage ("<<e->transform.position.x<<", "<<e->transform.position.z<<")\n";
        std::cout<<"  wt: px "<<wt.px<<" pz "<<wt.pz<<" xp "<<wt.xp<<" zp "<<wt.zp<<" nbpt "<<wt.nbpt<<", uv_ok "<<w->uv_ok<<" uv_chunk "<<w->uv_chunk<<", ct0 "<<w->ct0<<" ct1 "<<w->ct1<<"\n";
        check(wt.allocated&&wt.xp==int(w->xn-wt.px)+2&&wt.zp==int(w->zn-wt.pz)+2&&wt.nbpt==wt.xp*wt.zp&&wt.maxx==wt.xp&&wt.maxz==wt.zp,"wt_xp/wt_zp = (max-p)+2, maxx/maxz");
        check((f.events.flags[5][0]&0x40u),"bhObj005 setzt sys->st_flg 0x40");
        check(w->chunk_type==51&&w->normals.size()==w->base.size()&&w->cell.size()==w->base.size(),"vlist Chunk 51 mit Normalen");
        for(std::size_t i=0;i<w->base.size();++i)check(w->cell[i]==int(w->base[i].z)+int(w->base[i].x)*wt.zp,"Zelle je Eckpunkt = (int)z + (int)x * wt_zp");
        check(w->uv_ok,"npSetOffsetUV2 trifft den ersten Chunk 66 mit usr_num 0");
        check(w->ct3==0&&w->uv_u==0&&w->uv_v==0,"Init-Pass (case 0) ohne UV-Schritt");
        f.tick();
        check(w->ct3==1&&w->uv_u==256&&w->uv_v==0,"erster Pass in mode 1: npSetOffsetUV2(256, 0), ct3 1");
        for(int i=0;i<15;++i)f.tick();
        check(w->ct3==0&&w->uv_u==0&&w->uv_v==0,"16 Passes: Kachelversatz netto 0 (4x4-Atlas)");
        // Die 109-Erzeuger des Raums stellen 108-Ringe: innerhalb von 300 Passes Hoehen > 0,
        // verschobene Eckpunkte (y = Original + Zellhoehe) und veraenderte Normalen.
        bool raised=false,displaced=false,normal_changed=false,decayed=false;float peak=0;int ticks=17;
        for(int i=0;i<300;++i){f.tick();++ticks;float maxh=0;for(const float h:wt.wvp)maxh=std::max(maxh,h);if(maxh>0){raised=true;peak=std::max(peak,maxh);}
            if(raised&&maxh<peak)decayed=true;
            // Pass j = gfrm_ct & 1: gerade Passes schreiben das y der geraden Eckpunkte, ungerade das
            // der ungeraden, jeweils aus der Hoehe vor dem Abklingen dieses Passes (0..0.2 ueber h).
            const unsigned j=unsigned(ticks-1)&1u;
            for(std::size_t v=0;v<w->base.size();++v){if(w->vertices[v].y!=w->base[v].y)displaced=true;
                if((v&1u)==j){const int c=w->cell[v];check(c>=0&&c<wt.nbpt,"Zelle im Gitter");const float d=w->vertices[v].y-w->base[v].y-wt.wvp[std::size_t(c)];check(d>=-1e-5f&&d<=0.2f+1e-5f,"Eckpunkt-y = Original-y + Zellhoehe vor dem Abklingen (0..0.2 ueber der Zelle)");}
                if(w->normals[v].x!=w->base_normals[v].x||w->normals[v].z!=w->base_normals[v].z)normal_changed=true;}
        }
        std::cout<<"  Ringe: raised "<<raised<<" peak "<<peak<<" displaced "<<displaced<<" normals "<<normal_changed<<" decayed "<<decayed<<"\n";
        for(const auto& g:f.world.unbound_enemy_routines())std::cout<<"  Luecke: "<<g<<"\n";
        f.gaps("Ringe");
        check(raised&&displaced&&normal_changed&&decayed,"108-Ringe heben Zellen, verschieben Eckpunkte, aendern Normalen und klingen ab");
        // Normale eines ausgelenkten Eckpunkts: L1-normiert, y = -2/L1 (VU0_WAVE_CALC).
        for(std::size_t v=0;v<w->base.size();++v)if(w->normals[v].y!=w->base_normals[v].y){const auto& n=w->normals[v];check(std::fabs(std::fabs(n.x)+std::fabs(n.y)+std::fabs(n.z)-1.0f)<1e-4f&&n.y<0,"VU0_WAVE_CALC: |x|+|y|+|z| = 1, y < 0");break;}
    }
    water_wave()=WaterWaveField{};
    std::cout<<"water wave PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
