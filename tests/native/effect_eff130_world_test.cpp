// bhEff130 (effsub1.c:9001) type 4 (Waffe 18, Raketenwerfer) und type 6 (Waffe 20,
// Linear Launcher) im echten Raum RM_0060 (Zombieraum, Waende mit Raumhoehe/eigener Hoehe,
// aktive EN01). Alle Dienste (bhCheckL2Wall, bhCheckFlyAtari, sys->ef, rom->lgtp[0],
// plp->stflg, sys->ef_flg) sind die produktiven RoomWorld-Besitzer.
// Vorbedingungen (Diagnose, kein gespielter Fortschritt): der Geschosssatz wird wie der
// Abschuss player.c:4990-5125 mit WpnEffTab[w][2] und dem Versatz njCalcVector(R(wax,way,0),
// (0,0,-2.5|-3.0)) direkt in den Raumpool gelegt; sys->ef_flg 0x2 (playpch2.c:134) wird
// vorgesetzt, damit das Loeschen sichtbar ist. Zusaetzlich feuert der native CombatController
// beide Waffen echt (Inventar acquire/equip wie ein Fund). Semantikreferenz, nie kompiliert.
#include "world.h"
#include "room.h"
#include "combat.h"
#include "inventory.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include "weapon_tables.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace recvx;
static void check(bool b,const std::string& text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-3f){return std::fabs(a-b)<=eps;}
static bool close3(NJS_POINT3 a,NJS_POINT3 b,float eps=1e-3f){return close(a.x,b.x,eps)&&close(a.y,b.y,eps)&&close(a.z,b.z,eps);}
static NJS_POINT3 pos(const EffectSprite& s){return {s.px,s.py,s.pz};}
static NJS_POINT3 lo(const EffectSprite& s){return {s.lox,s.loy,s.loz};}
static int short_diff(int a,int b){return int(std::int16_t(std::uint16_t(a-b)));}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data,const char* room="RM_0060"):bytes(RoomArchive(data).load(std::string(room)+".RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data)),
        world(bytes,scene,actor,motion,frame,audio,data,{},room),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(true);
        world.set_camera_offset_owner(&camera);
        events.binding=[this](const EventCommand& c){return world.bind(c);};
        commit(); /* player.c:840 bhSetPlayer: bhCalcModel(plp) vor Event_init (runtime.cpp) */
        world.tick_camera(scene,scene.cameras.at(0),0); /* player.c:842 bhCheckCut(1) vor Event_init */
        events.initialize();
        world.control_camera(scene,scene.cameras.at(0));
    }
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else nodes=player.pose(motion,frame,actor.position,actor.yaw);world.commit_player_joints(player.script_nodes(nodes));}
    // game.c:50 bhControlEffect liest cam.vx/vy/vz des Vorbildes; game.c:58 bhControlCamera danach.
    void tick(){world.set_simulation_flags(events.flags[6][0]);world.tick();commit();world.control_camera(scene,scene.cameras.at(0));}
    const EffectSprite& sp(unsigned slot)const{return *world.effect_state(slot);}
    RECVX_ENEMY_WORK& enemy(unsigned i){auto* e=world.enemy_table().slot[i];check(e&&e->id==1u&&(e->flg&1u),"RM_0060: Zombie "+std::to_string(i)+" fehlt");return *e;}
    void no_gaps(const char* phase){
        bool bad=false;
        for(const auto& g:world.unbound_enemy_routines()){std::cerr<<phase<<": Luecke: "<<g<<'\n';bad=true;}
        check(!bad,std::string(phase)+": unbound_enemy_routines nicht leer");
    }
};
// Muendung: Spielerlage plus Hoehe 14.8 (die Hoehe, die der echte Abschuss aus owP[9]*wp_fps1
// in diesem Fixture fuer Waffe 18 liefert; combat_shot zeigt die echten Abschusslagen).
static NJS_POINT3 muzzle(const Fixture& f){return {f.actor.position.x,f.actor.position.y+14.8f,f.actor.position.z};}
// player.c:5096-5125: eft = WpnEffTab[w][2]; eft->ax = wax, eft->ay = way; fpsb = njCalcVector(R(wax,way,0), (0,0,z)).
static int fire(Fixture& f,unsigned weapon,NJS_POINT3 at,int ay,NJS_POINT3& fpsb){
    const EF_WORK& eft=WpnEffTab[weapon][2];
    check(eft.flg==0x04100001u&&eft.id==130&&eft.type==(weapon==18?4:6),"WpnEffTab[w][2] ist nicht bhEff130");
    EffectSpawn s;s.flg=eft.flg;s.id=eft.id;s.type=eft.type;s.flr_no=eft.flr_no;s.mdlver=eft.mdlver;
    s.position=at;s.scale={eft.sx,eft.sy,eft.sz};s.ax=0;s.ay=std::int16_t(ay);
    NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,0,std::int16_t(ay),0);
    NJS_VECTOR fps{0,0,weapon==20?-3.0f:-2.5f};njCalcVector(&m,&fps,&fpsb);            // :5098 / :5115
    s.has_offset=true;s.offset=fpsb;
    const int slot=f.world.spawn_unlinked_effect(s);
    check(slot>=0,"bhSetEffectTb: kein freier eff[]-Platz");
    const auto& o=f.sp(unsigned(slot));
    check(o.mode0==0&&close3(lo(o),fpsb,1e-6f)&&close3(pos(o),at,1e-6f),"bhSetEffectTb: lox/loy/loz = Versatz, Lage = eft->px");
    return slot;
}
// Winkel way, unter dem (0,0,-1) nach (dx,dz) zeigt.
static int way_to(float dx,float dz){return int(std::lround(std::atan2(-dx,-dz)*65536.0/(2.0*3.14159265358979)))&0xFFFF;}
static float seg_dist(NJS_POINT3 p,const RECVX_ENEMY_CAPSULE& c){
    const float ax=c.c2.x-c.c1.x,ay=c.c2.y-c.c1.y,az=c.c2.z-c.c1.z;
    const float l2=ax*ax+ay*ay+az*az;float t=l2>0?((p.x-c.c1.x)*ax+(p.y-c.c1.y)*ay+(p.z-c.c1.z)*az)/l2:0;t=t<0?0:(t>1?1:t);
    const float dx=p.x-(c.c1.x+t*ax),dy=p.y-(c.c1.y+t*ay),dz=p.z-(c.c1.z+t*az);return std::sqrt(dx*dx+dy*dy+dz*dz);
}
// :9131-:9292 die Spur eines Flugpasses. Neu angelegte Saetze erkennt der Test an Lage/Groessenbasis;
// je nach Platzordnung lief ihr erster Schritt im selben Pass schon (132 type 1: lo *= 0.8,
// px += lo vor dem Schrumpfen; 132 type 2: ct3 192 - 14, px += xn mit xn 0).
static void check_trail(const Fixture& f,unsigned slot,const EffectSprite& s){
    const NJS_POINT3 p=pos(s);
    if(s.type==4){
        // :9158-:9185 Bildwechsel nach ct3 & 1 (ct3 ist hier schon dekrementiert).
        const bool odd=s.ct3&1;
        check(close(s.tv[0].u,0.875f)&&close(s.tv[0].v,odd?0.0f:0.125f)&&close(s.tv[3].u,1.0f)&&close(s.tv[3].v,odd?0.125f:0.25f),"bhEff130 type 4 :9158: tv nach ct3 & 1");
        unsigned head=0,rings=0;
        NJS_MATRIX m;njUnitMatrix(&m);njRotateY(&m,s.ay);njRotateX(&m,s.ax);NJS_VECTOR z{0,0,1},zw;njCalcVector(&m,&z,&zw);
        for(unsigned k=0;k<512;++k){if(k==slot)continue;const auto* o=f.world.effect_state(k);if(!o||!(o->flg&1u)||o->id!=132)continue;
            if(o->type==0&&close3(pos(*o),p,1e-5f)){
                check(o->sxb==4.0f&&o->syb==4.0f&&o->szb==0.0f&&o->mdlver==0,"bhEff130 :9187: 132/0 sx 4, sy 4, sz 0, mdlver 0");++head;}
            if(o->type==1&&o->sxb==1.0f&&o->syb==1.0f&&o->szb==0.0f){
                NJS_POINT3 l=lo(*o),at=pos(*o);
                if(o->sx==0.8f){l={l.x/0.8f,l.y/0.8f,l.z/0.8f};at={at.x-l.x,at.y-l.y,at.z-l.z};}
                else if(o->sx!=1.0f)continue;
                if(!close3(at,p,1e-4f))continue;
                // :9212-:9230 vc (0,0.5,0.5) unter RY(ay) RX(ax) RZ(zufall): z-Anteil 0.5 entlang der lokalen z-Achse.
                const float len=std::sqrt(l.x*l.x+l.y*l.y+l.z*l.z),along=l.x*zw.x+l.y*zw.y+l.z*zw.z;
                check(close(len,std::sqrt(0.5f),1e-4f)&&close(along,0.5f,1e-4f),"bhEff130 :9220: 132/1 Versatz njCalcPoint(RY RX RZ,(0,0.5,0.5))");
                ++rings;}
        }
        check(head==1,"bhEff130 :9187: genau ein 132/0 an der Geschosslage je Flugpass ("+std::to_string(head)+")");
        check(rings==2,"bhEff130 :9220: genau zwei 132/1 je Flugpass ("+std::to_string(rings)+")");
    }else{
        const bool odd=s.ct3&1;
        check(close(s.tv[0].v,odd?0.0f:0.125f)&&close(s.tv[3].v,odd?0.125f:0.25f),"bhEff130 type 6 :9235: tv nach ct3 & 1");
        unsigned smoke=0;
        for(unsigned k=0;k<512;++k){if(k==slot)continue;const auto* o=f.world.effect_state(k);if(!o||!(o->flg&1u)||o->id!=132||o->type!=2)continue;
            if(!close3(pos(*o),p,1e-5f)||!(o->ct3==0||o->ct3==178))continue;
            check(o->sxb>=4.0f&&o->sxb<6.0f&&o->syb>=4.0f&&o->syb<6.0f&&o->szb==0.0f,"bhEff130 :9273: 132/2 sx/sy 4 + 2*rnd, sz 0");
            check(close(o->lox,0.2f*-s.lox,1e-5f)&&close(o->loz,0.2f*-s.loz,1e-5f),"bhEff130 :9283: vc.x/z = 0.2 * -lox/loz");
            check(o->loy>=0.2f*-(s.loy+0.25f)-1e-5f&&o->loy<=0.2f*-(s.loy-0.25f)+1e-5f,"bhEff130 :9284: vc.y = 0.2 * -(loy + 0.5 rnd - 0.25)");
            ++smoke;}
        check(smoke==((s.ct3%3)!=0?1u:0u),"bhEff130 :9264: 132/2 nur bei ct3 % 3 != 0 (ct3 "+std::to_string(s.ct3)+", "+std::to_string(smoke)+")");
    }
}
// Erster Pass (mode0 0 faellt in 1): :9015-:9069.
static void check_init(const EffectSprite& s,NJS_POINT3 at,NJS_POINT3 v){
    check(s.mode0==1&&s.tex_id==10&&s.ani_ct==(s.type==4?2u:0u),"bhEff130 :9055/:9062: tex_id 10, ani_ct 2 (type 4) / 0 (type 6)");
    check(s.ct3==99,"bhEff130 :9058/:9065: ct3 100, im ersten Flugpass -1");
    check(s.bl_src==8&&s.bl_dst==10&&s.tv[0].col==0xFFFFFFFFu&&s.tv[3].col==0xFFFFFFFFu,"bhEff130 :9017-:9035: Blend 8/10, weisse Ecken");
    check(s.gpx==at.x&&s.gpy==at.y&&s.gpz==at.z,"bhEff130 :9037: gp = Abschusslage");
    check(close3(pos(s),{at.x+v.x,at.y+v.y,at.z+v.z},1e-5f),"bhEff130 :9073: px += lox im ersten Pass");
}
struct Flight {unsigned passes=0;int ct1=-1,ct2=-1;bool wall=false;NJS_POINT3 apos{};};
// Fliegt bis mode0 2 und prueft je Pass :9073 (kein loy-Abzug ab type 4), :9090 Wand, :9109 ct3,
// :9318 Flugtreffer. Liefert den Ausgang.
static Flight fly(Fixture& f,unsigned slot,NJS_POINT3 at,NJS_POINT3 v,int target){
    Flight out;const auto type=f.sp(slot).type;
    for(unsigned pass=0;pass<110;++pass){
        const EffectSprite b=f.sp(slot);
        // Erwartung aus dem echten Wandsatz fuer die Strecke pxb -> pxb + lox.
        NJS_POINT3 p0=pass==0?at:pos(b),apos{};float len=0;CollisionWall atr{};int idx=-1;
        const bool wall=f.world.l2_wall(p0,lo(b),0x400u,&len,&apos,&atr,&idx)!=0;
        f.events.flags[14][0]&=~0x100u;
        const unsigned ef_before=f.world.effect_flags();
        f.tick();
        const EffectSprite& s=f.sp(slot);++out.passes;
        check((s.flg&1u)!=0,"bhEff130: Satz vor Ende freigegeben");
        if(pass==0)check_init(s,at,v);
        // :9079 kein loy -= 0.02 ab type 4. Nur der Gegnertreffer schreibt lox = lp.v (:9327),
        // die Differenz px - pxb, die von lox um Rundung abweichen darf.
        if(s.mode0!=2||s.ct2<0)check(s.lox==b.lox&&s.loy==b.loy&&s.loz==b.loz,"bhEff130 :9079: kein loy -= 0.02 ab type 4 (lox/loy/loz unveraendert)");
        if(wall){
            // :9090-:9105 Wandtreffer: 3 Einheiten vor apos zurueck, ct1 = ahtp->type, mode0 2, kein ct3--.
            NJS_VECTOR u=lo(b);njUnitVector(&u);
            check(s.mode0==2,"bhEff130 :9090: Wandtreffer setzt mode0 2");
            check(close3(pos(s),{apos.x-3.0f*u.x,apos.y-3.0f*u.y,apos.z-3.0f*u.z},1e-4f),"bhEff130 :9098: px = apos - 3 * unit(lp.v)");
            check(s.ct1==int(atr.type)&&s.ct2==-1,"bhEff130 :9102: ct1 = sys->ahtp->type");
            check(s.ct3==(pass==0?100:b.ct3),"bhEff130 :9104: Wandtreffer vor ct3--");
            const bool cleared=!(f.world.effect_flags()&0x2u);
            check(type==4?cleared:((ef_before&0x2u)?!cleared:true),"bhEff130 :9092: sys->ef_flg &= ~0x2 nur fuer type 4");
            check(!(f.events.flags[14][0]&0x100u),"bhEff130: plp->stflg 0x100 erst in mode0 2");
            out.wall=true;out.ct1=s.ct1;out.apos=apos;return out;
        }
        check(close3(pos(s),{p0.x+b.lox,p0.y+b.loy,p0.z+b.loz},1e-5f),"bhEff130 :9073: px += lox");
        check(s.ct3==(pass==0?99:b.ct3-1),"bhEff130 :9109: ct3--");
        if(s.mode0==2){
            // :9294-:9332 Flugtreffer: Kugel r = WpnTab[type + 14].r an px, Bewegung 2 * lp.v.
            check(target>=0&&s.ct2==target,"bhEff130 :9318: ct2 ist nicht der Zielgegner ("+std::to_string(s.ct2)+")");
            auto& e=f.enemy(unsigned(target));
            const float r=WpnTab[type+14].r;
            check(seg_dist(pos(s),e.watr)<=r+e.watr.r+1e-3f,"bhCheckFlyAtari: Treffer ohne Kugel/Kapsel-Beruehrung");
            check((e.flg2&0x4u)!=0,"bhEff130 :9325: ene[ct2].flg2 |= 0x4");
            check((e.flg&0x4u)!=0&&e.wpnr_no==int(type)+14,"weapon.c:1189/1206 ueber :9306: flg 0x4, wpnr_no = type + 14");
            check(close(e.dpx,s.px,1e-5f)&&close(e.dpy,s.py,1e-5f)&&close(e.dpz,s.pz,1e-5f),"weapon.c:1195 ueber :9296: dpx = gat.px (Geschosslage)");
            check(close(e.dvx,2.0f*b.lox,1e-5f)&&close(e.dvy,2.0f*b.loy,1e-5f)&&close(e.dvz,2.0f*b.loz,1e-5f),"weapon.c:1199 ueber :9302: dvx = 2 * lp.v");
            check(e.dax==s.ax&&e.day==s.ay,"weapon.c:1203 ueber :9314: dax/day = ax/ay");
            check(s.lox==s.px-p0.x&&s.loy==s.py-p0.y&&s.loz==s.pz-p0.z,"bhEff130 :9327: lox = lp.v = px - pxb");
            check(s.ct1==-1,"bhEff130: Gegnertreffer laesst ct1 -1");
            const bool cleared=!(f.world.effect_flags()&0x2u);
            check(type==4?cleared:((ef_before&0x2u)?!cleared:true),"bhEff130 :9320: sys->ef_flg &= ~0x2 nur fuer type 4");
            out.ct2=s.ct2;return out;
        }
        check(s.mode0==1&&s.ct2==-1,"bhEff130: im Flug mode0 1, ct2 -1");
        if(target>=0){auto& e=f.enemy(unsigned(target));
            check(seg_dist(pos(s),e.watr)>WpnTab[type+14].r+e.watr.r-1e-3f,"bhCheckFlyAtari: Beruehrung ohne Treffer");}
        check_trail(f,slot,s);
        check(f.world.effect_visible(slot),"bhEff130 :9377: Geschoss im Flug nicht gezeichnet (ef_trs)");
    }
    throw std::runtime_error("bhEff130: kein Treffer in 110 Passes");
}
// mode0 2 type 4 (:9938-:10033): SE 282, bhCheckFlyAtari r 10 (eidx ct2), 23 an px - cam.v,
// fuenf 22 mit Streuung, danach Freigabe.
static void blast4(Fixture& f,unsigned slot){
    const EffectSprite b=f.sp(slot);
    // cam.vx/vy/vz, die der Effektpass dieses Bildes liest: bhControlCamera des Vorbildes (game.c:50/58).
    const NJS_POINT3 cv=f.world.effect_context().camera_view;
    check(std::fabs(cv.x)+std::fabs(cv.y)+std::fabs(cv.z)>0.5f,"cam.vx/vy/vz nicht gesetzt (bhControlCamera)");
    f.events.flags[14][0]&=~0x100u;
    f.tick();
    const EffectSprite& s=f.sp(slot);
    check((f.events.flags[14][0]&0x100u)!=0,"bhEff130 :9381: plp->stflg |= 0x100");
    check((s.flg&1u)&&s.mode0==2&&s.mode1==1&&s.ct0==0,"bhEff130 :10027: mode1 1, ct0 0, Satz lebt");
    const NJS_POINT3 at{b.px-cv.x,b.py-cv.y,b.pz-cv.z};
    unsigned fire=0,splinters=0;
    for(unsigned k=0;k<512;++k){const auto* o=f.world.effect_state(k);if(!o||!(o->flg&1u)||k==slot)continue;
        if(o->id==23&&close3(pos(*o),at,1e-4f)){
            check(o->type==0&&o->mdlver==1&&o->flr_no==1&&o->sxb==4.0f&&o->syb==4.0f&&o->szb==4.0f,"bhEff130 :9965-:9980: 23 type 0, mdlver 1, flr_no 1, s 4");++fire;}
        if(o->id==22&&close3(pos(*o),at,1e-4f)){
            // :10004-:10024 Lage bleibt die der Vorlage (px - cam.v), sx = 2 + rnd, sy = sx, sz 0.
            check(o->type==0&&o->mdlver==1&&o->flr_no==1&&o->sxb>=2.0f&&o->sxb<3.0f&&o->syb==o->sxb&&o->szb==0.0f,"bhEff130 :10004: 22 type 0, mdlver 1, flr_no 1, sx 2 + rnd, sy = sx, sz 0");
            const int day=short_diff(o->ay,b.ay),dax=short_diff(o->ax,b.ax);
            check(day>=-14564&&day<=14564&&dax>=-10923&&dax<=10923,"bhEff130 :10015/:10016: ay +-80 Grad, ax +-60 Grad um die Geschossrichtung");
            NJS_MATRIX m;njUnitMatrix(&m);njRotateXYZ(&m,o->ax,o->ay,0);NJS_VECTOR z{0,0,1},vc1;njCalcVector(&m,&z,&vc1);
            check(close3(lo(*o),vc1,1e-5f),"bhEff130 :10020-:10024: Versatz njCalcVector(RXYZ(ax,ay,0),(0,0,1))");
            ++splinters;}
    }
    check(fire==1,"bhEff130 :9980: genau ein 23 an px - cam.v ("+std::to_string(fire)+")");
    check(splinters==5,"bhEff130 :9997: genau fuenf 22 ("+std::to_string(splinters)+")");
    f.tick();
    // Freigabe: der Platz haelt danach kein Geschoss mehr (er darf im selben Pass neu belegt werden).
    {const auto& o=f.sp(slot);check(!(o.flg&1u)||o.id!=130u||o.mode0!=2u,"bhEff130 :10031: mode1 1 gibt den Satz frei (flg 0)");}
}
// mode0 2 type 6 (:10036-:10095): SE 279, bhCheckFlyAtari r 10 / wpn_no 20, bhSetExplosionEffectEx(px - lox, 4),
// rom->lgtp[0], danach Freigabe.
static void blast6(Fixture& f,unsigned slot){
    const EffectSprite b=f.sp(slot);
    const NJS_POINT3 cv=f.world.effect_context().camera_view;                     // wie blast4
    check(std::fabs(cv.x)+std::fabs(cv.y)+std::fabs(cv.z)>0.5f,"cam.vx/vy/vz nicht gesetzt (bhControlCamera)");
    f.events.flags[14][0]&=~0x100u;
    f.tick();
    const EffectSprite& s=f.sp(slot);
    check((f.events.flags[14][0]&0x100u)!=0,"bhEff130 :9381: plp->stflg |= 0x100");
    check((s.flg&1u)&&s.mode0==2&&s.mode1==1,"bhEff130 :10068: mode1 1, Satz lebt");
    const NJS_POINT3 ps{b.px-b.lox,b.py-b.loy,b.pz-b.loz};                         // :10064-:10066
    const NJS_POINT3 at{ps.x-4.0f*cv.x,ps.y-4.0f*cv.y,ps.z-4.0f*cv.z};             // weapon.c:1888-1890
    unsigned fire=0;
    for(unsigned k=0;k<512;++k){const auto* o=f.world.effect_state(k);if(!o||!(o->flg&1u)||k==slot)continue;
        if(o->id==23&&close3(pos(*o),at,1e-4f)){
            check(o->type==2&&o->mdlver==1&&o->flr_no==0&&o->sxb==4.0f&&o->syb==4.0f&&o->szb==4.0f,"weapon.c:1876-1886: 23 type 2, mdlver 1, flr_no 0, s 4");++fire;}
    }
    check(fire==1,"bhEff130 :10067: bhSetExplosionEffectEx genau ein 23 an ps - 4 cam.v ("+std::to_string(fire)+")");
    const auto l=f.world.light_state(0);
    check(l.has_value(),"rom->lgtp[0] fehlt");
    check(l->type==101&&l->speed==8,"bhEff130 :10071-:10072: lgtp[0] type 101, aspd 8");
    check(close3(l->color,{4.5f,3.0f,1.5f},1e-6f)&&l->near_range==20.0f&&l->far_range==100.0f,"bhEff130 :10077-:10081: rgb 4.5/3/1.5, nr 20, fr 100");
    check(close3(l->position,ps,1e-5f),"bhEff130 :10082-:10084: lgtp[0] an ps = px - lox");
    check(l->flags==3u&&l->link==0u&&l->mode==0u,"bhEff130 :10070-:10076: lgtp[0] flg 3, lkflg 0, mode 0");
    f.tick();
    // Freigabe: der Platz haelt danach kein Geschoss mehr (er darf im selben Pass neu belegt werden).
    {const auto& o=f.sp(slot);check(!(o.flg&1u)||o.id!=130u||o.mode0!=2u,"bhEff130 :10093: mode1 1 gibt den Satz frei (flg 0)");}
}
static void wall_shot(const std::filesystem::path& data,unsigned weapon,float dx,float dz,int expect_type){
    Fixture f(data);f.tick();
    f.world.set_player_weapon(weapon);
    f.world.set_effect_flags(f.world.effect_flags()|0x2u);                          // playpch2.c:134
    const NJS_POINT3 at=muzzle(f);NJS_POINT3 v{};
    const int slot=fire(f,weapon,at,way_to(dx,dz),v);
    const auto r=fly(f,unsigned(slot),at,v,-1);
    check(r.wall&&r.ct1==expect_type,"Wandtreffer: Wandart "+std::to_string(r.ct1)+" statt "+std::to_string(expect_type));
    std::cout<<"  Waffe "<<weapon<<" Wand: "<<r.passes<<" Passes, apos "<<r.apos.x<<" "<<r.apos.y<<" "<<r.apos.z<<", ct1 "<<r.ct1<<'\n';
    if(weapon==18)blast4(f,unsigned(slot));else blast6(f,unsigned(slot));
    f.no_gaps(weapon==18?"type 4 Wand":"type 6 Wand");
}
static void enemy_shot(const std::filesystem::path& data,unsigned weapon,unsigned target){
    Fixture f(data);f.tick();
    f.world.set_player_weapon(weapon);
    f.world.set_effect_flags(f.world.effect_flags()|0x2u);                          // playpch2.c:134
    auto& e=f.enemy(target);const int hp=e.hp;
    const NJS_POINT3 at=muzzle(f);NJS_POINT3 v{};
    const int slot=fire(f,weapon,at,way_to(e.px-at.x,e.pz-at.z),v);
    const auto r=fly(f,unsigned(slot),at,v,int(target));
    check(!r.wall&&r.ct2==int(target),"Gegnertreffer erwartet");
    if(weapon==18)blast4(f,unsigned(slot));else blast6(f,unsigned(slot));
    for(unsigned i=0;i<20&&e.hp==hp;++i)f.tick();
    std::cout<<"  Waffe "<<weapon<<" Zombie "<<target<<": "<<r.passes<<" Passes, hp "<<hp<<" -> "<<e.hp<<'\n';
    check(e.hp<hp,"Gegnertreffer ohne Schaden (DmgCheck mit wpnr_no "+std::to_string(weapon)+")");
    f.no_gaps(weapon==18?"type 4 Gegner":"type 6 Gegner");
}
// Der echte Kampfpass: Raketenwerfer (Item 11 -> Waffe 18, sub1.c:4537) und Linear Launcher
// (Item 1 -> Waffe 20, sub1.c:4473) zielen und feuern mit dem nativen CombatController.
static void combat_shot(const std::filesystem::path& data,unsigned item,unsigned weapon){
    Fixture f(data);f.tick();
    // itemdata[1] (Linear Launcher) hat max 1 je Platz: eine Ladung; der Raketenwerfer nimmt 6.
    check(f.inv.acquire(item,item==1?1u:6u,0),"Werfer nicht aufgenommen");
    unsigned slot=0;for(unsigned i=1;i<12;++i)if(Inventory::id(f.inv.slots()[i])==item)slot=i;
    check(slot&&f.inv.equip(slot)&&f.inv.weapon()==weapon,"WeaponSet: Item "+std::to_string(item)+" ist nicht Waffe "+std::to_string(weapon));
    CombatController combat(data);
    int shell=-1;bool hit=false,freed=false;unsigned type=0;NJS_POINT3 gp{};
    int hp_sum0=0;for(unsigned i=0;i<4;++i){auto* e=f.world.enemy_table().slot[i];if(e&&(e->flg&1u))hp_sum0+=e->hp;}
    for(unsigned i=0;i<400&&!freed;++i){
        f.world.set_player_weapon(f.inv.weapon());f.world.set_player_way(combat.aim_yaw());f.world.set_player_attack(combat.attacking());
        f.tick();
        const CombatControls in{true,shell<0&&(i%40)==20,0,0,false};
        combat.tick(in,false,f.inv,f.player,f.actor,f.motion,f.frame,&f.world,f.audio,&f.events);
        if(shell<0){for(unsigned k=0;k<512;++k){const auto* o=f.world.effect_state(k);if(o&&(o->flg&1u)&&o->id==130){shell=int(k);type=o->type;gp={o->px,o->py,o->pz};}}}
        else{const auto& o=f.sp(unsigned(shell));if(!(o.flg&1u)||o.id!=130)freed=true;else if(o.mode0==2&&o.ct2>=0)hit=true;}
    }
    int hp_sum1=0;for(unsigned i=0;i<4;++i){auto* e=f.world.enemy_table().slot[i];if(e&&(e->flg&1u))hp_sum1+=e->hp;}
    std::cout<<"  Kampfpass Waffe "<<weapon<<": type "<<type<<" Abschuss "<<gp.x<<" "<<gp.y<<" "<<gp.z<<" Gegnertreffer "<<hit<<" freigegeben "<<freed<<" hp-Summe "<<hp_sum0<<" -> "<<hp_sum1<<'\n';
    check(shell>=0&&type==(weapon==18?4u:6u),"Kampfpass: kein bhEff130 type 4/6");
    check(hit&&freed&&hp_sum1<hp_sum0,"Kampfpass: Geschoss trifft keinen Zombie oder wird nicht freigegeben");
    f.no_gaps(weapon==18?"Kampfpass Waffe 18":"Kampfpass Waffe 20");
}
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    {   // Raumvoraussetzungen RM_0060: aktive Zombies 1 und 2, gesperrter Zombie 3 (stflg 0x1000000).
        Fixture f(data);f.tick();
        check(close3(f.actor.position,{40.3862f,0,3.01633f},1e-3f),"RM_0060: Startlage des Spielers anders");
        check(!(f.enemy(1).stflg&0x1000000u)&&(f.enemy(1).flg&0x20u),"RM_0060: Zombie 1 nicht trefferbereit");
    }
    std::cout<<"type 4 Wand (-x):\n";wall_shot(data,18,-1.0f,0.0f,4);
    std::cout<<"type 6 Wand (-x+z):\n";wall_shot(data,20,-1.0f,1.0f,0);
    std::cout<<"type 4 Gegner:\n";enemy_shot(data,18,1);
    std::cout<<"type 6 Gegner:\n";enemy_shot(data,20,1);
    std::cout<<"Kampfpass:\n";combat_shot(data,11,18);combat_shot(data,1,20);
    std::cout<<"effect_eff130_world: ok\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
