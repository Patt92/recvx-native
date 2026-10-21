// EN21 Albinoid-Jungtier im Originalraum RM_3120 (Typ 0, vom Raumskript ueber
// 0x24 inaktiv gehalten) und RM_3140 (Typ 1), echte RoomWorld-Dienste.
// Spielerlagen, das Freigeben ueber 0x24, die MV00-Zustandsschreibung und die
// Trefferfelder (wie weapon.c) sind Diagnosevorbedingungen, kein natuerlich
// gespielter Fortschritt. Erwartungen aus en21.c / zonzon1.c / effect.c.
#include "world.h"
#include "room.h"
#include "en21.h"
#include "en03_lib.h"
#include "combat.h"
#include "inventory.h"
#include "effect_sprites_internal.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static bool near(float a,float b,float eps=1e-4f){return std::fabs(a-b)<=eps;}
// en21.c:48 En21SparkTbl (Spielergelenke der Griffunken).
static const unsigned spark_joint[15]={17,21,16,20,15,19,14,18,1,2,3,6,10,4,5};
// en21.c:1181 uvinfo1 (Typ 1) und :1170/:1202 uvinfo0/uvinfo3 (Typ 0/3), je ohne Endeintrag.
struct Uv {int u,v,sx,sy;};
static const Uv spark_uv1[7]={{0,0,80,80},{80,0,80,80},{160,0,80,80},{0,80,80,80},{80,80,80,80},{160,80,80,80},{0,160,80,80}};
static const Uv spark_uv3[8]={{80,160,40,40},{120,160,40,40},{160,160,40,40},{200,160,40,40},{80,200,40,40},{120,200,40,40},{160,200,40,40},{200,200,40,40}};
// en21.c:1326 uvinfo0 der Spur, einschliesslich des Endeintrags {-1,0,0,0}.
static const Uv mark_uv[12]={{0,0,55,55},{55,0,55,55},{110,0,55,55},{165,0,55,55},{0,55,55,55},{55,55,55,55},{110,55,55,55},{165,55,55,55},{0,110,55,55},{55,110,55,55},{110,110,55,55},{-1,0,0,0}};
// effect.c:1279-1288 (bhEff_E21_Spark/Mark): tv[] aus einer UV-Zeile, sx/sy = sxb/syb * Breite / 32.
static bool quad_is(const EffectSprite& s,const Uv& uv,std::uint32_t col){
    for(const auto& t:s.tv)if(t.col!=col)return false;
    const float u0=uv.u/256.0f,v0=uv.v/256.0f,u1=(uv.u+uv.sx-1)/256.0f,v1=(uv.v+uv.sy-1)/256.0f;
    return near(s.tv[0].u,u0)&&near(s.tv[0].v,v0)&&near(s.tv[1].u,u1)&&near(s.tv[1].v,v0)&&near(s.tv[2].u,u0)&&near(s.tv[2].v,v1)&&near(s.tv[3].u,u1)&&near(s.tv[3].v,v1)
        &&near(s.sx,uv.sx/32.0f)&&near(s.sy,uv.sy/32.0f);
}
template<std::size_t N> static bool quad_in(const EffectSprite& s,const Uv (&table)[N],std::uint32_t col){for(const auto& uv:table)if(quad_is(s,uv,col))return true;return false;}
// {0, +-step, +-2*step}: (rand()%2 ? +step : -step) * (rand() % 3) (en21.c:1119-1124).
static bool spark_offset(float d,float step){for(int k=-2;k<=2;++k)if(near(d,step*float(k),2e-4f))return true;return false;}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;
    explicit Fixture(const std::filesystem::path& data,const char* room="RM_3120"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit();events.initialize();
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();}
    unsigned slot_of(unsigned id,unsigned nth=0){auto& t=world.enemy_table();for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==id&&nth--==0)return i;throw std::runtime_error("Original-EN21 fehlt");}
    RECVX_ENEMY_WORK& work(unsigned slot){return *world.enemy_table().slot[slot];}
    // 0x24 (kind 1, Satz index, an): der Raum gibt den Gegner frei (event.c bhInitModelSet).
    // In RM_3120 ist das Jungtier Satz 0 und Tabellenplatz 0.
    void activate(unsigned index){EventCommand c;c.opcode=0x24;c.bytes={0x24,1,std::uint8_t(index),1};check(world.bind(c),"0x24 nicht gebunden");}
    std::map<unsigned,EffectSprite> effects(unsigned id){std::map<unsigned,EffectSprite> out;for(unsigned i=0;;++i){const auto* s=world.effect_state(i);if(!s)break;if((s->flg&1u)&&s->id==id)out.emplace(i,*s);}return out;}
    RECVX_ENEMY_PLAYER player_work(){RECVX_ENEMY_PLAYER q{};check(world.enemy_player_get(q),"Spielerbesitzer fehlt");return q;}
    void place_player(NJS_POINT3 at,int floor){actor.position=at;commit();auto q=player_work();q.pos={at.x,at.y,at.z};q.flr_no=std::uint8_t(floor);q.flg&=~6u;check(world.enemy_player_set(q),"Spieler setzen");}
    NJS_POINT3 enemy_joint(unsigned slot,unsigned node){const auto m=world.enemy_slot_joint_matrix(slot,node);check(bool(m),"owP[node] des EN21 fehlt");return {m->value[12],m->value[13],m->value[14]};}
    NJS_POINT3 player_joint(unsigned joint){BoneMatrix m;check(world.enemy_player_joint(joint,m),"plp->owP[joint] fehlt");return {m.value[12],m.value[13],m.value[14]};}
    void no_gaps(const char* phase){
        // RM_3120: der vom Skript gehaltene Zombie (id 1, Typ 10) meldet seine eigene, vom
        // Jungtier unabhaengige Motionluecke (Stand 2026-10-20).
        for(const auto& g:world.unbound_enemy_routines()){
            if(g.find("(id 1, mtn 1)")!=std::string::npos)continue;
            throw std::runtime_error(std::string(phase)+": Luecke "+g);}
    }
};
static RECVX_EN21_EXP0& exp0_of(RECVX_ENEMY_WORK& e){check(e.exp0!=nullptr,"EXP0-Block fehlt (en21.c:159)");return *static_cast<RECVX_EN21_EXP0*>(e.exp0);}
static int16_t angle_to(const RECVX_ENEMY_WORK& e,float x,float z){return int16_t(int32_t(10430.381f*std::atan2(e.px-x,e.pz-z))-e.ay);} // en21.c:268-269

int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {
        // bhEne21_Init Typ 0 (en21.c:196-252) im Raumaufbau, gefolgt von bhSetMotion im selben Pass (:95).
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);auto& x=exp0_of(e);
        check(e.type==0,"RM_3120: Jungtier ist nicht Typ 0");
        check(e.mode0==1&&e.mode1==1&&e.mode2==1&&e.mode3==0,"Init: mode 1/1/1/0");
        check(e.hp==80,"Init: hp 80 (sys->gm_mode 0, en21.c:210-217)");
        check(e.ar==4.0f&&e.ah==1.0f&&e.aox==0.0f&&e.aoy==0.0f&&e.aoz==0.8f&&e.car==2.0f&&e.cah==1.0f,"Init: ar/ah/aox/aoy/aoz/car/cah");
        check((e.flg&0x978u)==0x978u&&!(e.flg&0x6u)&&(e.flg2&0x40u)&&!(e.stflg&0x8u),"Init: flg 0x178|0x800, flg2 0x40, stflg ohne 0x8, flg ohne 0x6");
        check(e.lok_jno==4&&e.mtn_no==7&&e.hokan_rate==65536&&e.mtn_add==65536&&e.mtn_md==0,"Init: lok_jno 4, mtn 7, hokan_rate/mtn_add 65536, mtn_md 0");
        // :231 frm_no = rand() % 19 ROH, danach :95 ein Motionschritt mtn_add 65536.
        check(e.frm_no>=65536&&e.frm_no<=65536+18,"Init: frm_no = rand()%19 roh plus ein Schritt 65536");
        check(e.clp_jno[0]==0&&e.clp_jno[1]==-1&&!(e.mdflg&0x20u),"Init: clp_jno 0/-1, mdflg ohne 0x20");
        {   // en21.c:28 CapColTab.
            static const int cap[9][3]={{1,4,10},{2,2,8},{-1,2,3},{3,3,8},{1,2,3},{5,10,5},{10,12,5},{12,14,4},{0,0,0}};
            check(e.cpcl!=nullptr,"Init: cpcl fehlt");
            for(int i=0;i<9;++i)check(e.cpcl[i].x==cap[i][0]&&e.cpcl[i].y==cap[i][1]&&e.cpcl[i].z==cap[i][2],"Init: cpcl != CapColTab");
        }
        check(x.player_dist==999.0f&&x.player_ang==0&&x.target_x==0&&x.target_y==0&&x.target_z==0&&x.in_view==0&&x.grab_wait==0&&x.death_mtn==4&&x.controlling==0,"Init: EXP0 999/0/0/0/0/0/4/0");
        // Raumskript haelt das Werk (0x24 aus: stflg 0x1000000, flg 0x8000), kein Move.
        check((e.stflg&0x1000000u)&&(e.flg&0x8000u),"RM_3120: Raumskript haelt das Jungtier nicht inaktiv");
        const float pz0=e.pz;const int ct0=e.ct0;f.tick();
        check(e.mode2==1&&e.mode3==0&&e.pz==pz0&&e.ct0==ct0,"inaktiv: das Jungtier bewegt sich");
        f.no_gaps("Init");

        // Freigabe; der Spieler steht weit hinter dem Jungtier (ausser Sicht). Erster Move:
        // Brain/SearchPlayer (:256), MV01 case 0+1: ct0 = rand%10+10, spd 0.5 + En21SpdAddTbl[1],
        // bhAddSpeed (pwksub.c:133), ausser Sicht -> mode2 2 oder 3 und return vor dem ct0-Abzug (:429-435).
        f.activate(slot);
        check(!(e.stflg&0x1000000u)&&!(e.flg&0x8000u),"0x24 an: stflg 0x1000000 / flg 0x8000 nicht geloest");
        const auto ply=f.player_work();const float px0=e.px;const float pzs=e.pz;
        f.tick();
        check(near(x.player_dist,std::sqrt((px0-ply.pos.x)*(px0-ply.pos.x)+(pzs-ply.pos.z)*(pzs-ply.pos.z)),1e-2f)&&x.target_x==ply.pos.x&&x.target_z==ply.pos.z,"SearchPlayer: Abstand / EXP0_F(0x8..0x10)");
        check(x.in_view==0&&std::abs(x.player_ang)>=18204,"SearchPlayer: Spieler hinter dem Jungtier ist nicht ausser Sicht (100 Grad)");
        check(near(e.spd,0.25f)&&near(e.pz,pzs-0.25f)&&near(e.px,px0),"MV01: spd 0.5 + En21SpdAddTbl[frm/65536 = 1], bhAddSpeed in -z");
        check(e.mode0==1&&e.mode1==1&&(e.mode2==2||e.mode2==3)&&e.mode3==0&&e.ct0>=10&&e.ct0<=19,"MV01 ausser Sicht: mode2 2/3, ct0 rand%10+10 ohne Abzug");
        const unsigned turn=e.mode2;const int ay0=e.ay;
        f.tick();
        check(e.mode2==turn&&e.mode3==1&&e.ct0>=9&&e.ct0<=18,"MV02/03 case 0+1: ct0 rand%10+10, Abzug");
        check(e.ay==ay0+(turn==2?-728:728),"MV02/03: ay -/+ NJM_DEG_ANG(4) = 728");
        f.no_gaps("Bewegung");
    }
    {
        // Verfolgung und Griff: Spieler 8 vor dem Jungtier (-z bei ay 0), in Sicht.
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);auto& x=exp0_of(e);
        f.activate(slot);
        f.place_player({e.px,e.py,e.pz-8.0f},e.flr_no);
        const int hp0=f.inv.hp;
        f.tick();
        check(x.in_view==1&&e.mode0==1&&e.mode2==1&&e.mode3==1,"Verfolgung: in Sicht bleibt MV01 (en21.c:415)");
        // :417-427: Zwischenpunkt aus bhCheckRoute oder Spielerlage; y bleibt die Spielerhoehe.
        {const auto q=f.player_work();check(x.target_y==q.pos.y,"SearchPlayer: EXP0_F(0xC) = plp->py");}
        // Bis zum Griff: Brain greift nur bei Abstand < 5.5, |Winkel| < NJM_DEG_ANG(5) und Sperrzeit 0.
        bool grabbed=false;int ticks=0;int before_grab_ay=0;
        while(!grabbed&&ticks<200){
            const auto q=f.player_work();before_grab_ay=q.ay;const float dx=e.px-q.pos.x,dz=e.pz-q.pos.z;const float dist=std::sqrt(dx*dx+dz*dz);
            const int16_t ang=angle_to(e,q.pos.x,q.pos.z);
            f.tick();++ticks;
            if(e.mode0==2){
                check(dist<5.5f&&std::abs(ang)<910,"Griff ausserhalb von Abstand 5.5 / Winkel 5 Grad (en21.c:297-299)");
                grabbed=true;}
            else check(!(dist<5.5f&&std::abs(int(ang))<910),"Griffbedingung erfuellt, aber kein Griff");
        }
        check(grabbed,"Verfolgung endet nicht im Griff");
        // NG00 case 0 (en21.c:753-776) und PlayerControl case 0 (:935-957) im selben Pass.
        auto q=f.player_work();
        check(x.controlling==1&&e.mode1==0&&e.mode2==0&&e.mode3==1&&e.mtn_no==7&&e.frm_no==65536&&e.ct0==0,"NG00: EXP0_UC(0x17) 1, mtn 7 frm 0 (+Schritt), ct0 0, mode3 1");
        check(q.mode0==4&&q.mode1==0&&q.mode3==1&&(q.flg&0x10004u)==0x10004u&&(q.stflg&0x10000u)&&(q.flg2&1u)&&!(q.flg&0x40000u),"NG00/PlayerControl: plp mode 4/0, flg 0x10004, stflg 0x10000, flg2 1");
        // hokan_count 5 (:955) laeuft im folgenden bhControlPlayer (game.c:49) weiter; hier nicht geprueft.
        check(q.mtn_no==(q.hp<30?44u:q.hp<120?43u:42u),"PlayerControl case 0: mtn 42/43/44 nach hp");
        // :759-768: |(short)(ay - plp->ay)| < 90 Grad -> mode2 1, ayp = ay; sonst mode2 0, ayp = (short)(ay + 180 Grad).
        check(q.mode2==(std::abs(int(int16_t(e.ay-before_grab_ay)))<16384?1u:0u),"NG00: plp->mode2 nach Blickrichtung");
        check(((q.ayp-(q.mode2==1?e.ay:e.ay+32768))&0xFFFF)==0,"NG00: plp->ayp = ay bzw. ay + 180 Grad");
        // Funken 399 Typ 1 je Pass mit geradem frm_no/65536 (:779-782), Gelenk En21SparkTbl[ct0].
        std::map<unsigned,EffectSprite> seen=f.effects(399);
        check(seen.size()==1,"NG00 ct0 0: ein Funke 399");
        std::map<unsigned,int> born;for(const auto& kv:seen)born[kv.first]=0;
        unsigned sparks=1;bool hit=false,released=false;int tick_no=0;
        RECVX_ENEMY_PLAYER before=q;
        while(!released&&tick_no<40){
            ++tick_no;const int ct0=e.ct0+1;const int frm=e.frm_no;const unsigned mode3=e.mode3;
            const NJS_POINT3 joint=ct0<15?f.player_joint(spark_joint[ct0]):NJS_POINT3{};
            // DIAGNOSE: sys->ef.ax/ay vorbelegt, damit "nicht geschrieben" (Typ 1, :1109-1125) sichtbar ist.
            effect_detail::request_template().ax=0x1234;effect_detail::request_template().ay=0x2345;
            const auto tmpl=effect_detail::request_template();
            const auto old=f.effects(399);before=f.player_work();const int hp_before=f.inv.hp;
            (void)f.world.take_vibration_requests();
            f.tick();
            const auto now=f.effects(399);
            std::vector<unsigned> fresh;for(const auto& kv:now)if(!old.count(kv.first))fresh.push_back(kv.first);
            // alte Funken: Typ 1 steigt je Pass 0.2 (:1263), nach drei Bildern frei (flm_tbl[1] = 3).
            for(const auto& kv:old){const auto it=now.find(kv.first);const int age=tick_no-born[kv.first];
                if(age>=3){check(it==now.end(),"Funke 399 lebt laenger als drei Bilder (flm_tbl 3)");born.erase(kv.first);continue;}
                check(it!=now.end(),"Funke 399 vor Ablauf frei");
                check(near(it->second.py,kv.second.py+0.2f)&&it->second.px==kv.second.px&&it->second.pz==kv.second.pz&&it->second.ct2==kv.second.ct2-1,"bhEff_E21_Spark Typ 1: py += 0.2, ct2--");
                check(quad_in(it->second,spark_uv1,0xFFA0A0FFu),"bhEff_E21_Spark Typ 1: UV aus uvinfo1[rand % 7]");}
            if(mode3==1){
                const bool even=((frm/65536)%2)==0;
                check(fresh.size()==(even?1u:0u),"NG00: Funke nur bei geradem frm_no/65536 (en21.c:779)");
                for(unsigned k:fresh){const auto& s=now.at(k);born[k]=tick_no;++sparks;
                    check(s.type==1&&(s.flg&0x180001u)==0x180001u&&(s.stflg&0x20u)&&s.tex_id==5&&s.bl_src==8&&s.bl_dst==10&&s.ct1==7&&s.ct2==2,"Funke 399: type 1, flg 0x180000, stflg 0x20, tex_id 5, Blend 8/10, ct1 pat 7, ct2 flm 3 - 1");
                    check(s.ax==0x1234&&s.ay==0x2345&&tmpl.ax==0x1234,"SetSparkEff Typ 1 schreibt sys->ef.ax/ay nicht (en21.c:1109-1125)");
                    check(near(s.sx,2.5f)&&near(s.sy,2.5f)&&quad_in(s,spark_uv1,0xFFA0A0FFu),"Funke 399: sx/sy 80/32, UV uvinfo1, Farbe 0xFFA0A0FF");
                    check(spark_offset(s.px-joint.x,0.2f)&&spark_offset(s.pz-joint.z,0.1f)&&spark_offset(s.py-0.2f-joint.y,0.1f),"Funke 399: Lage = plp->owP[En21SparkTbl[ct0]] + Zufallsversatz (en21.c:1114-1124)");
                    check(f.world.effect_visible(k),"Funke 399: nicht zeichenbar (txp[0]/tex_id)");}
            }
            const auto r=f.player_work();
            if(ct0==4&&mode3==1){
                // :784-802: hp - 9, plp mode 4/1/0, djnt_no 4, dpx = owP[0], way = ay, Stimme, Vibration.
                check(f.inv.hp==hp_before-9,"NG00 ct0 4: plp->hp -= 9");
                const unsigned mtn=r.mode2==0?74u:73u;const unsigned frames=f.player.motion_frames(mtn);
                check(r.mode0==4&&r.mode1==1&&r.mode3==1&&r.mtn_no==mtn,"NG00/PlayerControl case 1: mode 4/1, mtn 73/74");
                check(r.ct0==int(frames)-1&&r.ct1==int(frames)-3,"PlayerControl: ct0 = frm_num (-1 im selben Pass), ct1 = frm_num - 3");
                const auto vib=f.world.take_vibration_requests();bool v=false;for(const auto& p:vib)if(p.first==1&&p.second==10)v=true;
                check(v,"NG00: StartVibrationEx(1, 10)");
                // plp->djnt_no/dpx..dpz/way (:795-799) haben keinen oeffentlichen Leser am Raumbesitzer;
                // ihre Argumente prueft die Komponentenfixture en21_test.cpp.
                hit=true;
            }
            if(mode3==1&&ct0>4&&r.mode1==1){
                // :979-983 plp->ay += (short)(ayp - ay) / ct1 (ct1 > 0).
                if(before.ct1>0)check(((r.ay-(before.ay+int16_t(before.ayp-before.ay)/before.ct1))&0xFFFF)==0,"PlayerControl: ay-Annaeherung");
            }
            if(mode3==2){
                // NG00 case 2 (:809-827) und PlayerControl case 2 (:1008-1016).
                check(e.mode0==1&&e.mode1==1&&e.mode2==6&&e.mode3==0&&x.grab_wait==34&&x.controlling==0,"NG00 case 2: mode 1/1/6/0, EXP0_UC(0x15) 35 (-1 im Pass), 0x17 0");
                check(r.mode0==1&&r.mode1==2&&r.mode2==0&&r.mode3==0&&!(r.flg&0x10004u)&&(r.flg&0x8u)&&!(r.stflg&0x10000u)&&!(r.flg2&1u)&&r.at_flg==0,"PlayerControl case 2: Spieler frei");
                released=true;
            }
        }
        check(hit&&released,"Griffablauf unvollstaendig");
        check(sparks==8,"NG00: acht Funken (ct0 0,2,..,14)");
        check(f.inv.hp==hp0-9,"Griff: genau ein Abzug 9");
        f.no_gaps("Griff");
        // MV06 (:628) nach dem Griff mit AllWayWallCheck(3.0); Sperrzeit verhindert den naechsten Griff.
        // AllWayWallCheck (en21.c:1021-1082) unabhaengig ueber denselben bhCheckWallType-Besitzer.
        unsigned bits=0;{const int offs[4]={0,-16384,16384,32768};
            for(int k=0;k<4;++k){const int ang=(e.ay+offs[k])&0xFFFF;
                for(float i=1.0f;i<=3.0f;i+=2.0f)if(f.world.collision_contact({e.px-i*njSin(ang),e.py,e.pz-i*njCos(ang)},e.flg,e.ar,e.ah)){bits|=1u<<k;break;}}}
        f.tick();
        if(bits&0x8u){
            if((bits&0x6u)==0x6u)check(e.mode2==1&&e.mode3==0,"MV06: Wand hinten, links und rechts -> MV01");
            else check(e.mode2==6&&e.mode3==1&&e.mtn_no==((bits&0x2u)?2u:(bits&0x4u)?1u:e.mtn_no),"MV06: Viertelkreis");
        }else check(e.mode2==6&&e.mode3==2&&(e.mtn_no==1||e.mtn_no==2),"MV06: hinten frei -> Kehrtwende mtn 1/2");
        for(int i=0;i<33;++i){f.tick();check(e.mode0!=2,"Griff waehrend der Sperrzeit EXP0_UC(0x15)");}
        check(x.grab_wait==0,"Sperrzeit laeuft nicht ab");
        f.no_gaps("MV06/Sperrzeit");
    }
    {
        // Brain (en21.c:297-320): in Sicht, Abstand < 5.5, aber |Winkel| >= 5 Grad -> MV05 (Winkel > 0)
        // bzw. MV04, im selben Pass ay +/- NJM_DEG_ANG(7) = 1274 (:571/:608), spd 0.05 (:567/:604).
        for(int side=0;side<2;++side){
            Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);auto& x=exp0_of(e);
            f.activate(slot);
            const float dx=side==0?-1.5f:1.5f;f.place_player({e.px+dx,e.py,e.pz-3.0f},e.flr_no);
            const auto q=f.player_work();const int16_t ang=angle_to(e,q.pos.x,q.pos.z);const int ay0=e.ay;
            check(std::abs(int(ang))>=910&&std::abs(int(ang))<18204,"Testaufstellung: Winkel nicht zwischen 5 und 100 Grad");
            f.tick();
            const unsigned want=ang>0?5u:4u;
            check(x.in_view==1&&x.player_ang==ang&&e.mode0==1&&e.mode1==1&&e.mode2==want&&e.mode3==1,"Brain: |Winkel| >= 5 Grad -> MV05 (> 0) / MV04");
            check(e.ay==ay0+(want==5?1274:-1274)&&near(e.spd,0.05f)&&e.ct0>=9&&e.ct0<=18,"MV04/05: ay +/- 1274, spd 0.05, ct0 rand%10+10 - 1");
            f.no_gaps("MV04/05");
        }
        {   // Grenze NJM_DEG_ANG(5) = 910 (:299): Winkel ~4.7 Grad greift noch.
            Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);
            f.activate(slot);f.place_player({e.px-3.0f*std::tan(4.7f*3.14159265f/180.0f),e.py,e.pz-3.0f},e.flr_no);
            const auto q=f.player_work();const int16_t ang=angle_to(e,q.pos.x,q.pos.z);
            check(ang>728&&ang<910,"Testaufstellung: Winkel nicht zwischen 4 und 5 Grad");
            f.tick();
            check(e.mode0==2&&exp0_of(e).controlling==1,"Brain: |Winkel| < NJM_DEG_ANG(5) greift nicht");
        }
    }
    {
        // Spur 396 aus MV01..03 (en21.c:410/493/537, SetMarkEff :1299, bhEff_E21_Mark :1324).
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);
        f.activate(slot);
        int mark=-1;int tick_no=0;
        for(;tick_no<400&&mark<0;++tick_no){
            const auto old=f.effects(396);const NJS_POINT3 root=f.enemy_joint(slot,0);const int ay0=e.ay;const unsigned mode2=e.mode2,mode3=e.mode3;
            f.tick();
            for(const auto& kv:f.effects(396))if(!old.count(kv.first)){
                const auto& s=kv.second;
                // MV01: Spur vor dem Drehen (:412 vor :427); MV02/03 drehen vorher (:483/:527).
                const int ay=mode2==2?ay0-728:mode2==3?ay0+728:ay0;(void)mode3;
                check(s.type==0&&(s.flg&0x80001u)==0x80001u&&(s.stflg&0x20u)&&s.tex_id==6&&s.bl_src==11&&s.bl_dst==3,"Spur 396: type 0, flg 0x80000, stflg 0x20, tex_id 6, Blend 11/3");
                check(s.ax==16384&&((s.ay-ay)&0xFFFF)==0,"Spur 396: ax NJM_DEG_ANG(90), ay = epw->ay");
                check(near(s.px,root.x)&&near(s.py,root.y+0.1f)&&near(s.pz,root.z),"Spur 396: Lage owP[0] + 0.1 y");
                check(s.ct1==32&&s.ct2==0&&quad_is(s,mark_uv[0],0x40808080u),"Spur 396: ct1 33 - 1, ct2 0, UV uvinfo0[0], Farbe 0x40808080");
                check(f.world.effect_visible(kv.first),"Spur 396: nicht zeichenbar (txp[0]/tex_id 6)");
                if(mark<0)mark=int(kv.first);
            }
        }
        check(mark>=0,"keine Spur 396 aus MV01..03");
        // Lebensdauer: ct1 32..0 (33 Bilder), ct2++ bei ct1 % 3 == 0, letzter Eintrag ist {-1,0,0,0}.
        int ct1=32,ct2=0;
        for(int i=0;i<32;++i){
            f.tick();const auto* s=f.world.effect_state(unsigned(mark));
            check(s&&(s->flg&1u)&&s->id==396,"Spur 396 vor ct1 < 0 frei");
            --ct1;if(ct1%3==0)++ct2;
            check(s->ct1==ct1&&s->ct2==ct2&&quad_is(*s,mark_uv[ct2],0x40808080u),"bhEff_E21_Mark: ct1/ct2/UV-Folge");
        }
        check(ct2==11,"Spur: Endeintrag nicht erreicht");
        f.tick();{const auto* s=f.world.effect_state(unsigned(mark));check(!s||!(s->flg&1u)||s->id!=396||s->ct1==32,"Spur 396 lebt nach ct1 < 0 weiter");}
        f.no_gaps("Spur");
    }
    {
        // MV00 (en21.c:344) mit Funke Typ 3 bei ct0 == 8. DIAGNOSE: mode2 0 per Zustandsschreibung
        // (natuerlich nur nach MV01-Ablauf mit rand()%10 == 0, :442-445). Spieler weit hinten.
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);
        f.activate(slot);f.tick();
        e.mode0=1;e.mode1=1;e.mode2=0;e.mode3=0;
        f.tick();
        check(e.mode2==0&&e.mode3==1&&near(e.spd,0.05f)&&e.ct0>=9&&e.ct0<=18,"MV00: spd 0.05, ct0 rand%10+10 - 1");
        bool fired=false;
        for(int i=0;i<20&&!fired;++i){
            const int ct0=e.ct0;const auto old=f.effects(399);const NJS_POINT3 j1=f.enemy_joint(slot,1);const int ay=e.ay;
            f.tick();
            std::vector<unsigned> fresh;for(const auto& kv:f.effects(399))if(!old.count(kv.first))fresh.push_back(kv.first);
            check(fresh.size()==(ct0==8?1u:0u),"MV00: Funke 399 nur bei ct0 == 8 (en21.c:364)");
            for(unsigned k:fresh){const auto s=*f.world.effect_state(k);
                const int ang=(ay+32768)&0xFFFF;
                check(s.type==3&&s.ct1==8&&s.ct2==2&&s.tex_id==5&&(s.stflg&0x20u)&&s.bl_src==8&&s.bl_dst==10,"Funke Typ 3: ct1 pat 8, ct2 flm 3 - 1, tex_id 5, Blend 8/10");
                check((s.ay&0xFFFF)==ang,"Funke Typ 3: sys->ef.ay = epw->ay + NJM_DEG_ANG(180)");
                check(near(s.px,j1.x-njSin(ang)*0.7f,2e-4f)&&near(s.py,j1.y+0.7f)&&near(s.pz,j1.z-njCos(ang)*0.7f,2e-4f),"Funke Typ 3: owP[1] + 0.7 y, ein Schritt 0.7 in -sin/-cos(ay)");
                check(near(s.sx,1.25f)&&quad_in(s,spark_uv3,0xFFA0A0FFu),"Funke Typ 3: uvinfo3, sx 40/32");
                fired=true;}
        }
        check(fired,"MV00: kein Funke Typ 3");
        f.no_gaps("MV00");
    }
    {
        // Schaden und Tod (en21.c:840/885), Blutpfuetze zonzon1.c:391 mit BloodType[21].color.
        // DIAGNOSE: Trefferfelder wie weapon.c (dam[j], flg 0x4) statt eines gezielten Schusses.
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);auto& x=exp0_of(e);
        f.activate(slot);f.tick();
        const std::size_t particles0=f.world.blood_particles();
        e.dam[3]=30;e.flg|=4u;
        f.tick();
        check(e.hp==50&&!(e.flg&4u)&&e.dam[3]==0,"Damage: hp - 30, dam[]/flg 0x4 am Passende geloescht");
        check(f.world.blood_particles()>particles0,"Damage: bhEne_SetBloodEffect2 ohne Partikel");
        e.dam[5]=51;e.flg|=4u;
        f.tick();
        check(e.mode0==4&&e.mode1==0&&e.mode2==0&&e.mode3==0&&e.hp==50,"Damage: hp < dam -> mode 4/0/0/0, hp unveraendert (en21.c:864-869)");
        // DD00 case 0+1 (:892-921).
        f.tick();
        check(!(e.flg&0x8u)&&e.mtn_no==4&&x.death_mtn==5&&e.mode3==1&&e.hokan_count<=3,"DD00: flg ohne 0x8, mtn EXP0_UC(0x16) = 4, naechste 5");
        const int left=e.ct0;check(left>=0,"DD00: ct0 = frm_num - 1");
        const auto pools0=f.effects(300).size();
        for(int i=0;i<left;++i){f.tick();check(e.mode3==1&&e.mtn_add==65536,"DD00: Pfuetze vor ct0 == 0");}
        check(f.effects(300).size()==pools0,"DD00: Pfuetze vor ct0 == 0");
        f.tick();
        check(e.mode3==2&&e.mtn_add==0,"DD00 ct0 0: mtn_add 0, mode3 2");
        const auto pools=f.effects(300);
        check(pools.size()==pools0+1,"bhEne_BloodPool: kein eff 300");
        for(const auto& kv:pools)check(kv.second.flg==0x200001u,"bhSetBloodPoolLnk: flg 0x200001");
        f.tick();
        check(e.hp==-1&&(e.flg&0x2u),"DD00 case 2: hp -1, flg 0x2");
        // bhEff300 zeichnet mit ani_ct = pal_bnk = BloodType[21].color (effsub3.c:199) aus dem Atlas 17.
        {
            const auto* atlas=f.world.effect_group_textures(17);check(atlas&&!atlas->empty(),"Effektatlas 17 fehlt");
            const unsigned color=recvx_t3_blood_type[21].color;check(color==0,"BloodType[21].color ist 0 (zonzon1.c:31)");
            Scene out;f.world.append(out,{},SceneCamera{});
            // Bildinhalt statt Zeiger: der Zeichenpass legt eigene Kopien der Atlasbilder an.
            check(atlas->size()>1&&!((*atlas)[0].pixels==(*atlas)[1].pixels),"Atlas 17: Paletten 0/1 nicht unterscheidbar");
            bool found=false,other=false;
            for(const auto& t:out.triangles)if(t.texture<out.textures.size()){
                if(out.textures[t.texture].pixels==(*atlas)[color].pixels)found=true;
                for(std::size_t k=0;k<atlas->size();++k)if(k!=color&&out.textures[t.texture].pixels==(*atlas)[k].pixels)other=true;}
            check(!other,"Blutpfuetze zeichnet mit einer fremden Palette aus Atlas 17");
            check(found,"Blutpfuetze zeichnet nicht mit Atlas 17 Bild BloodType[21].color");
        }
        f.no_gaps("Tod");
    }
    {
        // Echter Waffentreffer: CombatController (player_combat_core.c) -> enemy_table -> dam[]/flg 0x4.
        // VORBEDINGUNG: Handfeuerwaffe (Item 9, WeaponSet -> Waffe 4) per acquire/equip, Spieler 14 vor
        // dem Jungtier, Zielen nach unten. Erwartung: Schaden nur im Move (en21.c:332), im Griff
        // loescht :102 dam[] ohne Abzug; am Ende DD00 mit Pfuetze, hp -1, flg 0x2.
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);
        f.activate(slot);
        check(f.inv.acquire(9,15,0),"Item 9 nicht aufgenommen");
        unsigned item=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==9)item=i;
        check(item&&f.inv.equip(item)&&f.inv.weapon()==4u,"WeaponSet: Item 9 ist nicht Waffe 4");
        f.place_player({e.px,e.py,e.pz-14.0f},e.flr_no);
        CombatController combat(data);
        const auto pools0=f.effects(300).size();
        bool moving_hit=false,grab_hit_ignored=false;unsigned pending=0;
        for(unsigned i=0;i<900&&!(e.flg&2u);++i){
            const int before=e.hp;const unsigned m0=e.mode0;
            // DIAGNOSE: Magazin nachgefuellt (Nachladen ist nicht Gegenstand dieses Tests).
            if(Inventory::quantity(f.inv.slots()[item])==0)f.inv.slots()[item]=Inventory::pack(9,15)|(f.inv.slots()[item]&0xFF000000u);
            f.world.set_player_weapon(f.inv.weapon());f.world.set_player_way(combat.aim_yaw());f.world.set_player_attack(combat.attacking());
            f.tick();
            // Ein Treffer des Kampfteils wirkt im naechsten Gegnerpass (game.c:48 vor dem Spielerteil).
            if(pending){
                if(m0==1&&e.mode0==1&&e.hp<before)moving_hit=true;
                if(m0==2&&e.hp==before)grab_hit_ignored=true;
                check(!(e.flg&4u),"Treffer: flg 0x4 nach dem Gegnerpass nicht geloescht");}
            const unsigned hits=combat.hits;
            combat.tick(CombatControls{(i%32)<28,(i%32)>=12&&(i%8)==4,0,-1,false},false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
            pending=combat.hits-hits;
        }
        check(combat.hits>0&&moving_hit,"kein echter Waffentreffer mit hp-Abzug im Move");
        check(e.hp==-1&&(e.flg&2u),"echter Waffentod: hp -1, flg 0x2 nicht erreicht");
        check(f.effects(300).size()==pools0+1,"echter Waffentod: keine Blutpfuetze");
        std::cout<<"  Waffe 4: Schuesse="<<combat.shots<<" Treffer="<<combat.hits<<(grab_hit_ignored?" (Treffer im Griff ohne Abzug)":"")<<'\n';
        f.no_gaps("Waffentod");
    }
    {
        // Toedlicher Griff (en21.c:787-791, :829-835): plp->hp < 0 -> mode3 3, danach mode 1/0/0/0.
        Fixture f(data);const unsigned slot=f.slot_of(21);auto& e=f.work(slot);
        f.activate(slot);
        f.place_player({e.px,e.py,e.pz-4.0f},e.flr_no);
        f.inv.hp=5;
        int i=0;for(;i<10&&e.mode0!=2;++i)f.tick();
        check(e.mode0==2,"toedlicher Griff: kein Griff");
        for(i=0;i<10&&e.mode3!=3;++i)f.tick();
        check(e.mode3==3&&f.inv.hp==-4,"NG00: hp 5 - 9 < 0 -> mode3 3, Abzug bleibt stehen");
        f.tick();
        check(e.mode0==1&&e.mode1==0&&e.mode2==0&&e.mode3==0&&e.ct0==0,"NG00 case 3: ct0 0, mode 1/0/0/0");
        f.tick();
        check(e.mode0==1&&e.mode1==0&&e.mode2==0&&e.mode3==1&&e.ct0>=9&&e.ct0<=18&&near(e.spd,0.05f),"nach case 3: MV00 ohne Brain (mode1 0)");
        f.no_gaps("toedlicher Griff");
    }
    {
        // RM_3140: Typ-1-Jungtiere (en21.c:162-195) unter Ereignissteuerung (mode0 5).
        Fixture f(data,"RM_3140");unsigned n=0;
        auto& t=f.world.enemy_table();
        for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i){auto* w=t.slot[i];if(!w||w->id!=21||!(w->stflg&0x8u))continue;++n;
            auto& x=exp0_of(*w);
            check(w->type==1&&!(w->flg&0x178u)&&!(w->flg&0x6u)&&!(w->flg2&1u)&&w->ar==0&&w->ah==0&&w->car==0&&w->cah==0&&w->hp==80,"Init Typ 1: flg ohne 0x178/0x6, flg2 ohne 1, ar/ah/car/cah 0, hp 80");
            check(x.player_dist==999.0f&&x.death_mtn==4&&x.controlling==0,"Init Typ 1: EXP0");}
        check(n==6,"RM_3140: sechs Typ-1-Jungtiere");
        for(int i=0;i<60;++i)f.tick();
        f.no_gaps("RM_3140");
        // DIAGNOSE (Zustandsschreibung): ein Jungtier im Griffabschluss NG00 case 2 bzw. im Tod DD00 case 0,
        // um die Verteilung an ALLE lebenden Works der Identitaet 21 (en21.c:815-822, :899-911) zu pruefen.
        std::vector<unsigned> infants;for(unsigned i=0;i<t.ewk_n&&i<RECVX_ENEMY_SLOTS;++i)if(t.slot[i]&&t.slot[i]->id==21&&(t.slot[i]->flg&1u))infants.push_back(i);
        check(infants.size()==7,"RM_3140: sieben lebende Jungtiere");
        const unsigned k=infants[3];auto& g=f.work(k);
        g.mode0=2;g.mode1=0;g.mode2=0;g.mode3=2;exp0_of(g).controlling=1;
        f.tick();
        for(unsigned i:infants)check(exp0_of(f.work(i)).grab_wait==(i<k?35u:34u),"NG00 case 2: EXP0_UC(0x15) = 35 an alle Jungtiere (spaetere Works zaehlen im selben Bild herunter)");
        auto& d=f.work(infants[5]);d.mode0=4;d.mode1=0;d.mode2=0;d.mode3=0;exp0_of(d).death_mtn=6;
        f.tick();
        check(d.mtn_no==6,"DD00: mtn_no = EXP0_UC(0x16) 6");
        for(unsigned i:infants)check(exp0_of(f.work(i)).death_mtn==4,"DD00: 6 + 1 > 6 -> 4 an alle Jungtiere (en21.c:900-911)");
        // Das ereignisgesteuerte Jungtier spielt die Raummotionen des Skripts (0x30 bhSetEneMtn,
        // mtn_room 1); dort gibt es keinen Eintrag 6. In dieser kuenstlichen Aufstellung ist die
        // benannte Luecke mnwP[6].frm_num die korrekte Antwort, kein Ersatzwert.
        for(const auto& gap:f.world.unbound_enemy_routines()){
            if(gap=="mnwP[mtn_no].frm_num"&&d.mtn_room)continue;
            if(gap.find("(id 1, mtn 1)")!=std::string::npos)continue;
            throw std::runtime_error("RM_3140 Verteilung: Luecke "+gap);}
    }
    std::cout<<"EN21 world PASS\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
