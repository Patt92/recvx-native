// Original RM_5810 spawns (EN13 slot 0, EN14 slot 1 as type 0) on the actual
// RoomWorld services. Expectations are taken from en14.c (bhEne14_Init :225,
// MV00 :360, MV01 :378, MV02 :411, Acid :1311, HitMark :1198) and from the
// shared helpers zonzon1.c:698 bhEne_GetShapeCnt / :864 bhArcTan2, which this
// test re-implements on its own. State writes below are diagnostic
// preconditions (the EN13 drive en13.c:490-493/532-535 and the hit fields the
// combat core leaves behind), not naturally played progression. EN14 type 1
// does not exist in any shipped room (scan over data/rooms: RM_5810/RM_9302 both
// type 0), so the flying states MV04/MV11 and PlayerControl are not reachable here.
#include "world.h"
#include "room.h"
#include "en13.h"
#include "en14.h"
#include "ninja_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static bool close(float a,float b,float eps=1e-3f){return std::fabs(a-b)<eps;}
// zonzon1.c:864 bhArcTan2: asin of y/len, mirrored through (short)(32768 - ang) for x < 0.
static int arc_tan2(float y,float x){
    const float len=std::sqrt(x*x+y*y);if(len==0.0f)return 0;
    int ang=int(10430.381f*std::asin(y/len));
    if(x<0.0f)ang=short(32768-ang);
    return ang;
}
// zonzon1.c:698 bhEne_GetShapeCnt on the ShapeTbl rows of en14.c:125-141.
struct ShapeRow{int key;float cnt;};
static const ShapeRow acid_front[5]={{0,0},{5,1000},{12,1000},{20,0},{999,0}};
static const ShapeRow acid_side[6]={{0,0},{6,1000},{10,300},{16,1000},{25,0},{999,0}};
static float shape_cnt(const ShapeRow* p,int key){
    const ShapeRow* pp;
    for(;;){pp=p;if(key<=pp->key)break;++p;}
    if(pp->key==key)return p->cnt;
    return p[-1].cnt+(((p->cnt-p[-1].cnt)*float(key-p[-1].key))/float(pp->key-p[-1].key));
}
struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;
    std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;PlayerAsset player;
    Inventory inv;MessageDialog dialog;std::uint32_t head=0;RoomWorld world;EventVm events;NJS_POINT3 camera{};
    Fixture(const std::filesystem::path& data):bytes(RoomArchive(data).load("RM_5810.RDX",64*1024*1024)),scene(decode_scene(bytes)),actor(scene.spawns.at(0)),audio(data),player(PlayerAsset::load(data,10)),
        world(bytes,scene,actor,motion,frame,audio,data,{},"RM_5810"),events(bytes,inv,dialog){
        world.set_player_asset(&player);world.bind_player_collision(events);world.bind_enemy_player(inv.hp,head);world.set_enemy_ai(true);world.set_enemy_grab(false);
        world.set_camera_offset_owner(&camera);
        // player.c:840 bhCalcModel(plp) before Event_init's bhControlEnemy (event.c:352), as the runtime does
        // (LookPlayaer :996 reads plp->mlwP->owP[5] in the first pass when the script leaves flg 0x100000 set).
        commit();
        events.binding=[this](const EventCommand& c){return world.bind(c);};events.initialize();
    }
    // While an enemy drives the player (plp->mnwP = epw->mnwP, player.c:1502 / en13.c PlayerControl) the
    // pose comes from the world's sampler, as en03/en29 world tests do; otherwise from the player library.
    // Test setup, not a port claim: after bhEne13_PlayerControl releases the player (mode0 1, plp->mnwP = mnwPb)
    // mtn_no still indexes the enemy library until bhControlPlayer picks a new motion; this fixture has no player
    // controller, so the last committed pose stays (as plp->mlwP->owP would until the next bhCalcModel(plp)).
    void commit(){std::vector<BoneMatrix> nodes;if(world.player_scripted)(void)world.sample_player(player,&nodes);else if(player.has_motion(motion))nodes=player.pose(motion,frame,actor.position,actor.yaw);else return;world.commit_player_joints(player.script_nodes(nodes));}
    RECVX_ENEMY_WORK& e13(){auto* p=world.enemy_table().slot[0];check(p&&p->id==13,"original RM_5810 Alexia II (EN13) missing");return *p;}
    RECVX_ENEMY_WORK& e14(){auto* p=world.enemy_table().slot[1];check(p&&p->id==14&&p->type==0,"original RM_5810 EN14 type 0 missing in slot 1");return *p;}
    RECVX_EN14_STATE& s14(){auto* s=static_cast<RECVX_EN14_STATE*>(e14().exp0);check(s,"bhEne14_Init: exp0 (bhEne_CallocWork 584) missing");return *s;}
    void tick(){commit();world.set_simulation_flags(events.flags[6][0]);world.tick();}
    // Diagnostic precondition: en13.c br00 would otherwise drive en14->mode2 itself
    // (MV01 timer[0], MV03 timer[1], MV02 timer[2]); large timers keep EN13 idle.
    void quiet_en13(){auto* s=static_cast<RECVX_EN13_STATE*>(e13().exp0);check(s,"EN13 exp0 missing");for(auto& t:s->timer)t=1<<20;}
    // mnwP[mtn].frm_num of the EN14 library (room.c:375 epw->mnwP = sys->emtp[id]). The sv-> services are
    // only valid inside the enemy pass; enemy_player_motion_frames(mtn, true) reads the library of
    // ene[sys->enow], and the pass rewrites enow for every work, so pointing it at slot 1 here is harmless.
    unsigned frames(unsigned mtn){world.enemy_table().enow=1;const unsigned n=world.enemy_player_motion_frames(mtn,true);check(n>0,"mnwP[mtn_no].frm_num is 0 in RM_5810");return n;}
    std::set<unsigned> alive(unsigned id){std::set<unsigned> out;for(unsigned k=0;k<600;++k){const auto* s=world.effect_state(k);if(!s)break;if((s->flg&1u)&&s->id==id)out.insert(k);}return out;}
    void no_en14_gaps(const char* where){
        for(const auto& g:world.unbound_enemy_routines())
            if(g.find("EN14")!=std::string::npos||g.find("bhEne14")!=std::string::npos)
                throw std::runtime_error(std::string(where)+": EN14 gap reported: "+g);
    }
};
int main(int argc,char** argv){try{
    check(argc==2,"data required");_Make_SinTable();const std::filesystem::path data=argv[1];
    // --- bhEne14_Init (:225-321) runs inside Event_init (event.c:352 bhControlEnemy, here events.initialize());
    //     the fixture's first tick is therefore the MV00 pass (:360-376). ---
    {
        Fixture f(data);f.tick();auto& a=f.e13();auto& e=f.e14();auto& s=f.s14();
        check(e.mode0==1&&e.mode1==0&&e.mode2==0&&e.mode3==1,"en14.c:245-256 type 0 -> mode0 1/mode1 0/mode2 0, :360-369 MV00 -> mode3 1");
        // :230-232 flg |= 0x78, &= ~6, flg2 |= 1; :297 loescht 0x10 wieder, sobald der Typ-0-Satz seinen EN13 gefunden hat.
        if(!((e.flg&0x78u)==0x68u&&!(e.flg&6u)&&(e.flg2&1u)))throw std::runtime_error("en14.c:230-232/:297: flg |= 0x78, &= ~6, &= ~0x10 (EN13 gefunden), flg2 |= 1 (flg "+std::to_string(e.flg)+" flg2 "+std::to_string(e.flg2)+")");
        check(e.ar==8.0f&&e.ah==25.0f&&e.car==25.0f,"en14.c:233-236: ar 8, ah 25, car written twice (4 then 25)");
        check(e.hp==400,"en14.c:237: hp 400");
        // en14.c:181-187: bhEne14_Mode0 (MV00 :360-369 setzt hokan_count 18, frm_no 0) laeuft VOR bhSetMotion
        // im selben Pass; Motion.c:338/578/853 (SetMtn*Hokan) hokan_count--, Motion.c:87 frm_no += add
        // -> am Ende des MV00-Passes 17 und 65536.
        if(!(e.mtn_no==0&&e.mtn_add==65536&&e.hokan_count==17&&e.frm_no==65536&&e.hokan_rate==45875&&!(e.mtn_md&2u)&&(e.flg&0x100000u)))throw std::runtime_error("en14.c:360-369 MV00 + Motion.c:338/:87 im selben Pass: mtn 0, hokan 18-1/45875, frm_no 65536, flg 0x100000 (mtn_no "+std::to_string(e.mtn_no)+" hokan_count "+std::to_string(e.hokan_count)+" hokan_rate "+std::to_string(e.hokan_rate)+" mtn_md "+std::to_string(e.mtn_md)+" frm_no "+std::to_string(e.frm_no)+")");
        static const int8_t clip[8]={6,1,7,9,11,15,18,21};
        for(int k=0;k<8;++k)check(e.clp_jno[k]==clip[k],"en14.c:280-287: clp_jno");
        const auto* tp=static_cast<const uint8_t*>(e.mtn_tp);
        check(tp&&tp[6]==6&&tp[7]==9&&tp[8]==10&&tp[9]==7&&tp[10]==8&&tp[11]==11,"en14.c:18 flip_tree: wings 7..10 mirrored");
        check(!(e.mdflg&0x20u),"en14.c:288: mdflg &= ~0x20");
        check(e.lkwkp==&a&&e.lkono==0&&e.ay==a.ay&&!(e.flg&0x10u)&&!(e.flg&0x80u),"en14.c:296-310: linked to the EN13 work, lkono 0, ay copied, flg 0x10/0x80 cleared");
        check(close(e.px,a.px)&&close(e.py,a.py)&&close(e.pz,a.pz),"en14.c:304-306: position copied from EN13");
        check((e.stflg&8u)&&!(e.flg&0x800u),"en14.c:319-321: type 0 -> stflg |= 8, no bhSetShadow");
        for(int i=0;i<11;++i)check(s.length[i]==2.5f&&s.velocity[i].x==0&&s.velocity[i].y==0&&s.velocity[i].z==0,"en14.c:1086-1104 TailInit: length 2.5, velocity 0");
        check(s.look_ticks==0&&s.beam==0,"en14.c:213-221: no bhEff130 type 4 in the pool, EXP0 0x244 NULL");
        f.no_en14_gaps("Init");
    }
    
    // --- MV01 (:378-410): the EN13 drive en13.c:490-493 sets mode2 1. ---
    {
        Fixture f(data);f.tick();f.quiet_en13();for(int i=0;i<4;++i)f.tick();
        auto& e=f.e14();auto& s=f.s14();const unsigned n=f.frames(1);
        e.mode2=1;e.mode3=0;
        f.tick();
        check(!(e.flg&0x100000u)&&s.look_ticks==15,"en14.c:383-384: flg 0x100000 cleared, EXP0 0x240 = 16 (LookPlayaer :1052 counts to 15 in the same pass)");
        // :385-388 hokan_count 8, danach Motion.c:338 hokan_count-- im selben Pass (en14.c:187).
        check(e.mtn_no==1&&e.hokan_count==7&&e.hokan_rate==45875,"en14.c:385-388 + Motion.c:338: motion 1, hokan 8-1/45875");
        check(e.frm_no==65536,"bhSetMotion in the same pass: frm_no 0 -> 65536");
        check(e.ct0==int(n)-2,"en14.c:389/:395: ct0 = frm_num - 1, post-decremented once in the same pass");
        unsigned ticks=1;bool relit=false;
        while(e.mode2==1&&ticks<n+5){const int before=e.frm_no;f.tick();++ticks;
            if(before==2949120){check((e.flg&0x100000u)&&s.look_ticks==15,"en14.c:401-404: frame 45 -> flg 0x100000, EXP0 0x240 = 16");relit=true;}}
        check(ticks==n&&e.mode1==0&&e.mode2==0&&e.mode3==0,"en14.c:395-399: back to MV00 after frm_num passes");
        if(n>46)check(relit,"en14.c:401: frm_no 2949120 never seen although motion 1 is longer than 45 frames");
        f.no_en14_gaps("MV01");
    }
    
    // --- MV02 (:411-497) with bhEne14_Acid (:1311-1402): the EN13 drive en13.c:532-535 sets mode2 2. ---
    const auto spit=[&](float offset_deg,int expect_ct1){
        Fixture f(data);f.tick();f.quiet_en13();for(int i=0;i<4;++i)f.tick();
        auto& a=f.e13();auto& e=f.e14();auto& s=f.s14();
        // Player 30 units in front of EN13, turned by offset_deg (diagnostic precondition).
        const float th=(float(a.ay)+offset_deg*65536.0f/360.0f)*(6.2831853f/65536.0f);
        f.actor.position={a.px-30.0f*std::sin(th),a.py,a.pz-30.0f*std::cos(th)};
        RECVX_ENEMY_PLAYER pl;check(f.world.enemy_player_get(pl),"player view");
        // :420-433 recomputed here: ang = (short)(bhArcTan2(ep->px - plp->px, ep->pz - plp->pz) - ep->ay).
        const int ang=short(arc_tan2(a.px-pl.pos.x,a.pz-pl.pos.z)-a.ay);
        const int ct1=ang<-5461?1:(ang>5461?2:0);
        check(ct1==expect_ct1,"test setup: player placement does not yield the intended ct1");
        const unsigned mtn=ct1?3:2;const unsigned n=f.frames(mtn);
        e.mode2=2;e.mode3=0;
        auto before=f.alive(256);
        f.tick();
        check(e.ct1==ct1&&e.mtn_no==mtn&&(((e.mtn_md&2u)!=0)==(ct1==2)),"en14.c:420-446: ct1 by +/-30 deg, motion 2 (front) or 3 (side), mtn_md 2 only for ct1 2");
        check(!(e.flg&0x100000u)&&s.look_ticks==7,"en14.c:418-419: flg 0x100000 cleared, EXP0 0x240 = 8 (7 after LookPlayaer)");
        // :448-456 hokan_count 8 und ct0 = frm_num - 1 (Durchfall in case 1: ct0-- :483), danach Motion.c:338 hokan_count--.
        check(e.hokan_count==7&&e.hokan_rate==45875&&e.ct0==int(n)-2&&(e.mdflg&2u),"en14.c:448-456 + Motion.c:338: hokan 8-1/45875, ct0 frm_num - 1 (- 1), mdflg 2");
        const auto* ent=f.world.state(1,1);
        check(ent&&ent->morph_obj_a==0&&ent->morph_obj_b==1,"en14.c:452-454: mlwP = mdl, obj_a = mdl[0].objP, obj_b = mdl[1].objP");
        check(e.shp_ct==0.0f&&f.alive(256)==before,"en14.c:462-470: frame 0 -> shape 0, no acid yet");
        unsigned ticks=1,spat=0;bool lit=false;
        while(e.mode2==2&&ticks<n+5){
            const int fno=e.frm_no/65536; // frame at the start of the pass
            const auto mouth=f.world.enemy_joint_position(1,6);check(mouth.has_value(),"owP[6] of EN14");
            // :1329-1340: v.y = sqrt(0); t = floor((v.y + sqrt(v.y^2 - 0.6 (plp->py - p.y))) / 0.3), at least 1.
            check(mouth->y>pl.pos.y,"test setup: the player must stand below the mouth joint (otherwise the original flight time is NaN)");
            float t=std::floor(std::sqrt(-0.6f*(pl.pos.y-mouth->y))/0.3f);if(t<1.0f)t=1.0f;
            const float vx=(pl.pos.x-mouth->x)/t,vz=(pl.pos.z-mouth->z)/t;const float speed=std::sqrt(vx*vx+vz*vz);
            before=f.alive(256);f.tick();++ticks;
            const auto after=f.alive(256);std::vector<unsigned> fresh;
            std::set_difference(after.begin(),after.end(),before.begin(),before.end(),std::back_inserter(fresh));
            const bool spit_frame=fno>=7&&fno<13;
            check(fresh.size()==(spit_frame?4u:0u),"en14.c:472-475/:1353-1355: four Eff256 per frame 7..12 (type 0), none otherwise");
            for(unsigned k:fresh){const auto* sp=f.world.effect_state(k);
                check(sp->type==4&&(sp->stflg&0x20u)&&sp->tex_id==0&&sp->mdlver==1,"en14.c:1355/:1370-1374/:1393: type 4, stflg 0x20, tex_id 0, mdlver = se 1");
                // bhSetEffectTb copies size into sx/sy/sz and sxb/syb (effect.c:466-470); the effect pass of the same
                // frame (game.c:50 after :48) runs bhEff_E03_Acid, whose bhEff_SetUVInfo rescales sx/sy from sxb/syb
                // (effsub2.c:1389 ff.), so the untouched witnesses are sxb, syb and sz.
                check(sp->sxb==sp->syb&&sp->syb==sp->sz&&sp->sz>=1.0f&&sp->sz<1.5f,"en14.c:1362-1366: size 1 + 0.5 rand on all three axes (sxb/syb/sz)");
                // The effect pass of the same tick already moved the drop once (bhEff_E03_Acid: p += v, v *= 0.97, yn -= 0.3).
                float x=sp->xn,y=sp->yn,z=sp->zn,px=sp->px,py=sp->py,pz=sp->pz;
                if(sp->mode0!=0){x/=0.97f;y=(y+0.3f)/0.97f;z/=0.97f;px-=x;py-=y;pz-=z;}
                check(close(std::sqrt(x*x+y*y+z*z),speed,2e-2f),"en14.c:1376-1384: njRotateY/njRotateX keep the launch speed");
                const float dx=px-mouth->x,dy=py-mouth->y,dz=pz-mouth->z;
                check(std::sqrt(dx*dx+dy*dy+dz*dz)<=speed+2e-2f,"en14.c:1388-1390: start = mouth + rand * direction");
            }
            spat+=unsigned(fresh.size());
            check(close(e.shp_ct,shape_cnt(ct1?acid_side:acid_front,fno)),"en14.c:462-470: shp_ct from ShapeTbl_Acid_F/S at the frame of the pass");
            if(fno==15){check((e.flg&0x100000u)&&s.look_ticks==3,"en14.c:477-480: frame 15 -> flg 0x100000, EXP0 0x240 = 4 (3 after LookPlayaer)");lit=true;}
        }
        check(ticks==n&&e.mode1==0&&e.mode2==0&&e.mode3==0&&!(e.mdflg&2u),"en14.c:483-489: after frm_num passes mode1/2/3 0, mdflg 2 cleared");
        check(spat==24,"six spit frames with four drops each");
        if(n>16)check(lit,"en14.c:477: frame 15 never seen");
        f.no_en14_gaps("MV02");
    };
    spit(0.0f,0);spit(45.0f,2);spit(-45.0f,1);
    
    // --- bhEne14_HitMark (:1198-1310) through EN13 (en13.c:759-782, type 0 damage goes to the parent). ---
    {
        Fixture f(data);f.tick();f.quiet_en13();for(int i=0;i<4;++i)f.tick();
        auto& a=f.e13();auto& e=f.e14();check(a.mode0==1,"EN13 must be in bhEne13_Move for InitDamage");
        const auto hit=[&](int wpn,int joint,int d1,int d3){
            e.flg|=4u;e.wpnr_no=wpn;e.djnt_no=joint;e.comb_flg=(e.comb_flg&~0x70u)|0x10u; // near range from the combat core
            std::memset(e.dam,0,sizeof e.dam);e.dam[1]=d1;e.dam[3]=d3;e.dvx=e.dvy=e.dvz=0.0f;
            const int hp13=a.hp;const auto bursts=f.alive(297).size();
            f.tick();
            check(!(e.flg&4u),"en13.c:759: flg 4 consumed by EN13");
            check(e.hp==400,"en14.c:1288-1291: type 0 keeps its hp");
            check((e.comb_flg&4u)&&!(e.comb_flg&8u),"zonzon1.c bhEne_CalcDamage: hit vector 0 -> z >= 0 -> comb_flg 4");
            check(f.alive(297).size()==bursts+1,"en14.c:1225-1240: one bhEne_SetBloodEffectBurst (297) per hit with DmgReact type >= 0");
            return hp13-a.hp;};
        // Weapon 3: CombWepTbl[3] = {20, {6,4,2}, 60, 0}; two hit joints 1 and 3 -> 12 points, no crit.
        e.comb_wep=0;e.comb_pnt=0;e.comb_timeout=0;
        check(hit(3,1,30,20)==50,"en13.c:761-782: EN13 hp -= total_dam 50");
        // comb_timeout = 60 (CombWepTbl[3]) in bhEne14_HitMark aus dem EN13-Pass (ene[0]); EN14s eigener Pass (ene[1])
        // endet mit bhEne_InitDamage (eneset.c:379, zonzon1.c:818-821 comb_timeout--) -> 59 am Pass-Ende.
        check(e.total_dam==50&&e.comb_wep==3&&e.comb_pnt==12&&e.comb_timeout==59&&!(e.comb_flg&1u),"zonzon1.c:725 bhEne_CalcDamage with CombWepTbl[3]: 6 + 6 points, timeout 60 - 1 (bhEne_InitDamage), no crit");
        // Weapon 2: CombWepTbl[2] = {4, {1,0,0}, 120, 800}; four near hits of 10 -> crit on the fourth (+800 %).
        e.comb_flg=0;e.comb_wep=0;e.comb_pnt=0;e.comb_timeout=0;
        for(int i=0;i<3;++i){check(hit(2,4,10,0)==10,"en13.c: EN13 hp -= 10 per plain hit");check(e.comb_pnt==i+1&&e.comb_timeout==119&&!(e.comb_flg&1u),"CombWepTbl[2]: one point per near hit, timeout 120 - 1 (bhEne_InitDamage zonzon1.c:820)");}
        check(hit(2,4,10,0)==90,"CombWepTbl[2]: 4 points reach crit 4 -> 10 + 10 * 800 / 100 = 90");
        check((e.comb_flg&1u)&&e.comb_pnt==0&&e.total_dam==90,"bhEne_CalcDamage: comb_flg 1, points reset (comb_flg 2 unset)");
        f.no_en14_gaps("HitMark");
    }
    
    // --- Natural run: the EN13 brain drives EN14 (en13.c:490/532); no EN14 gap over 600 passes. ---
    {
        Fixture f(data);unsigned spat=0,spit_passes=0;
        for(int i=0;i<600;++i){const auto before=f.alive(256);f.tick();const auto after=f.alive(256);
            unsigned fresh=0;for(unsigned k:after)if(!before.count(k))++fresh;if(fresh){spat+=fresh;++spit_passes;}}
        std::cout<<"  RM_5810 natural 600 passes: "<<spat<<" Eff256 in "<<spit_passes<<" passes, EN14 mode2 "<<unsigned(f.e14().mode2)<<", hp "<<f.e14().hp<<'\n';
        f.no_en14_gaps("natural run");
    }
    std::cout<<"EN14 init, retract, spit/acid, hitmark and natural run PASS\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}return 0;}
