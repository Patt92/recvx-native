// Skriptbefehle 0x6e bhHEffectSet (event.c:5138 -> effect.c:548 bhSetEffectEvt) und 0x4a
// bhYakkyouSet (event.c:3113 -> pwksub.c:3242 bhSetYakkyou) am Raumbesitzer RoomWorld::bind,
// mit den echten Befehlsbytes der Originalraeume. Die Erwartungen werden hier unabhaengig aus den
// Originalzeilen nachgerechnet (eigener Dekoder, eigenes njCalcPoint/atan2f), nicht aus world.cpp.
// Datenstellen (Byte-Scan der Ereignisabschnitte aller 205 Raeume, Treffer am Befehlskontext
// geprueft): 0x6e in RM_3130 Ereignis 0 @2642 (bhEff_Sub398), RM_1090 Ereignis 12 @3228/@3250,
// RM_0030 Ereignis 5 @3316, RM_0070 Ereignis 1 @1774, RM_3100 Ereignis 1 @2636, RM_3140 Ereignis 2
// @1882 (alle bhEff113); 0x4a nur mit v0 1 (Gegnerwork) in RM_0030, RM_2050, RM_3000, RM_3010,
// RM_3100, RM_3210, RM_6060, RM_6100.
#include "world.h"
#include "events.h"
#include "inventory.h"
#include "messages.h"
#include "asset_io.h"
#include "actor.h"
#include "weapon_tables.h"
#include "ninja_math.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace recvx;
static void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}

namespace {
std::uint32_t le32(const std::vector<std::uint8_t>& b,std::size_t at){return std::uint32_t(b.at(at))|std::uint32_t(b.at(at+1))<<8|std::uint32_t(b.at(at+2))<<16|std::uint32_t(b.at(at+3))<<24;}

struct Fixture {
    std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn actor;std::size_t motion=42;std::int64_t frame=0;AudioMixer audio;RoomWorld world;
    Fixture(const std::filesystem::path& data,const char* name):bytes(read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024)),scene(decode_scene(bytes)),
        actor(scene.spawns.empty()?SceneSpawn{}:scene.spawns.at(0)),audio(data),world(bytes,scene,actor,motion,frame,audio,data,{},name){}
    // EVT_WORK beginnt an u32 Kopf+28 (room.cpp inspect_room); Befehlsoffsets wie EVTRACE relativ dazu.
    std::vector<std::uint8_t> command(std::size_t offset,std::size_t length,unsigned opcode)const{
        const std::size_t at=le32(bytes,28)+offset;check(at+length<=bytes.size()&&bytes[at]==opcode,"room record is not the expected opcode");
        return {bytes.begin()+std::ptrdiff_t(at),bytes.begin()+std::ptrdiff_t(at+length)};}
    std::set<unsigned> active()const{std::set<unsigned> s;for(unsigned i=0;const auto* e=world.effect_state(i);++i)if(e->flg&3u)s.insert(i);return s;}
    unsigned first_free()const{unsigned i=0;for(;const auto* e=world.effect_state(i);++i)if(!(e->flg&3u))return i;return i;}
    std::vector<unsigned> added(const std::set<unsigned>& before)const{std::vector<unsigned> out;for(const auto i:active())if(!before.count(i))out.push_back(i);return out;}
    bool bind(unsigned opcode,std::size_t offset,const std::vector<std::uint8_t>& b){EventCommand c;c.opcode=opcode;c.task=0;c.event=0;c.offset=offset;c.bytes=b;return world.bind(c);}
};

// event.c:5138-5257 bhHEffectSet, nachgerechnet Zeile fuer Zeile ueber einen eigenen Zeiger.
struct HEffect {unsigned type,effno;float px,py,pz,ox,oy,oz;int ax,ay;};
HEffect decode_heffect(const std::vector<std::uint8_t>& cmd){
    const std::uint8_t* p=cmd.data();HEffect h{};
    const auto u16=[](const std::uint8_t* q){return unsigned(q[0])|unsigned(q[1])<<8;};
    p+=2;const unsigned v1=*p;++p;const unsigned v0=*p;++p;
    h.px=(v0&1)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    h.py=(v0&2)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    h.pz=(v0&4)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    const int v2=*p;++p;int v3=*p;if(v2&1)v3=-v3;
    h.ax=int(float(v3)*(65536.0f/360.0f));
    ++p;int v4=*p;if(v2&2)v4=-v4;h.ay=int(float(v4)*(65536.0f/360.0f));
    ++p;const int v5=*p;++p;
    h.ox=(v5&1)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    h.oy=(v5&2)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    h.oz=(v5&4)?-1.0f*(float(u16(p))/100.0f):float(u16(p))/100.0f;p+=2;
    h.effno=u16(p);h.type=v1;return h;}

// effect.c:548-599 bhSetEffectEvt: der Satz nach npSetMemoryL und den Zuweisungen.
void expect_evt(const EffectSprite& s,const HEffect& h,const std::string& where){
    check(s.flg==1u,where+": flg differs from effect.c:557 (1)");
    check(s.id==h.effno&&s.type==h.type,where+": id/type differ from effect.c:559/561");
    check(s.tex_id==-1&&s.mdlver==0&&s.flr_no==0,where+": tex_id/mdlver/flr_no differ from effect.c:563-567");
    check(s.px==h.px&&s.py==h.py&&s.pz==h.pz,where+": px/py/pz differ from the u16/100 decode");
    check(s.lox==h.ox&&s.loy==h.oy&&s.loz==h.oz,where+": lox/loy/loz differ from pnt->ox/oy/oz (effect.c:573-575)");
    check(s.sx==1.0f&&s.sy==1.0f&&s.sz==1.0f,where+": sx/sy/sz differ from effect.c:577-579");
    check(s.sxb==0.0f&&s.syb==0.0f&&s.szb==0.0f,where+": sxb/syb/szb not left zero (bhSetEffectEvt writes no base scale)");
    check(s.ax==h.ax&&s.ay==h.ay&&s.az==0,where+": ax/ay/az differ from effect.c:581-583 (int degrees)");
    check(s.lkwkp==nullptr&&s.lkono==0&&s.link_kind==-1,where+": lkwkp/lkono differ from effect.c:587-589");}

// ps2_NaMatrix.c:1252 njCalcPoint: Zeilen 0..2 mal x/y/z plus Zeile 3.
NJS_POINT3 calc_point(const float m[16],NJS_POINT3 v){return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};}
bool near(float a,float b){return std::fabs(a-b)<=1e-4f*std::max(1.0f,std::fabs(b));}

const float* owp(const RoomWorld& w,unsigned index,unsigned node){
    const auto* e=w.state(1,index);check(e&&e->enemy_native&&e->enemy_calculated&&e->enemy_asset,"room enemy has no committed owP");
    const unsigned physical=e->enemy_asset->model_index(node);check(physical<e->enemy_owp_mtx.size(),"owP node outside the committed tree");
    return e->enemy_owp_mtx[physical].value;}

void heffect_room(const std::filesystem::path& data,const char* room,std::initializer_list<std::size_t> offsets,std::vector<HEffect>* decoded=nullptr){
    Fixture f(data,room);
    for(const auto offset:offsets){
        const auto cmd=f.command(offset,22,0x6e);const auto h=decode_heffect(cmd);if(decoded)decoded->push_back(h);
        const auto before=f.active();const unsigned slot=f.first_free();
        check(f.bind(0x6e,offset,cmd),std::string(room)+": 0x6e not bound by RoomWorld::bind");
        const auto added=f.added(before);
        check(added.size()==1&&added[0]==slot,std::string(room)+": bhSetEffectEvt did not take the first free eff[512] slot");
        expect_evt(*f.world.effect_state(slot),h,std::string(room)+" @"+std::to_string(offset));
        check(!f.world.unbound_enemy_routines().count("bhEff"+std::to_string(h.effno)),std::string(room)+": effect procedure reported as unported");}
}
// pwksub.c:3242-3369 bhSetYakkyou(ene[...], v3, v1, v2, v6) nachgerechnet; bei wno 8 laeuft der
// Befehl hoechstens zweimal, bis die Handstatik einen geraden Stand hat (siehe RM_6060 unten).
void yakkyou_check(Fixture& f,std::size_t offset,const std::vector<std::uint8_t>& cmd,bool diagnostic_pose,const std::string& where){
    const unsigned v4=cmd[2],v1=cmd[3],v2=cmd[4],v3=cmd[5];
    if(diagnostic_pose){
        auto& e=const_cast<WorldEntity&>(*f.world.state(1,v4));check(e.enemy_asset!=nullptr,where+": no enemy asset");
        auto& j=e.enemy_owp_mtx.at(e.enemy_asset->model_index(v1)).value;auto& r=e.enemy_owp_mtx.at(e.enemy_asset->model_index(0)).value;
        const float c=std::cos(0.6f),sn=std::sin(0.6f);
        const float jm[16]={c,0,-sn,0, 0,1,0,0, sn,0,c,0, 1.5f,12.0f,-3.25f,1};const float rm[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, -2.0f,0.0f,4.0f,1};
        std::memcpy(j,jm,sizeof jm);std::memcpy(r,rm,sizeof rm);}
    const float* jm=owp(f.world,v4,v1);const float* rm=owp(f.world,v4,0);
    const EF_WORK eft=WpnEffTab[v3][3],eft2=WpnEffTab[v3][2];const auto& wt=WpnTab[v3];
    // :3276-3299 ops/wps je Hand (wno 22 hat eigene Konstanten, kommt in den Raumdaten nicht vor).
    NJS_POINT3 ops=wt.wp_cps,wps=wt.wp_fps1;if(v2!=0){ops.x=-ops.x;wps.x=-wps.x;}
    const NJS_POINT3 jps=calc_point(jm,wps);
    // :3301-3305 jps = owP[0] - jps (x/z), ay = (int)(10430.381f * atan2f(x, z)) in EF_WORK.ay (short).
    const int ay=int(std::int16_t(int(10430.381f*std::atan2(rm[12]-jps.x,rm[14]-jps.z))));
    NJS_POINT3 ops2=wt.wp_cps;if(v2!=0)ops2.x=-ops2.x;ops2.z+=eft2.sz;const NJS_POINT3 smoke=calc_point(jm,ops2); // :3313-3330
    for(unsigned attempt=0;;++attempt){
        const unsigned yk=f.world.effect_counter();const auto before=f.active();
        check(f.bind(0x4a,offset,cmd),where+": 0x4a not bound by RoomWorld::bind");
        const auto added=f.added(before);
        if(added.empty()){check(v3==8&&attempt==0&&f.world.effect_counter()==yk,where+": no shell although pwksub.c:3248 does not skip");continue;}
        check(added.size()==2,where+": bhSetYakkyou did not allocate shell [3] and smoke [2]");
        const auto* shell=f.world.effect_state(added[0]);const auto* puff=f.world.effect_state(added[1]);
        check(shell->id==eft.id&&shell->id==0&&shell->flg==(0x40601u|0x80u)&&shell->type==eft.type&&shell->mdlver==eft.mdlver,where+": shell flg/id/type differ from pwksub.c:3272 + bhSetEffectTb link");
        check(shell->flr_no==int(yk)&&f.world.effect_counter()==((yk+1)&0x1fu),where+": sys->yk_ct not used/advanced (pwksub.c:3274/3311)");
        check(shell->sx==eft.sx&&shell->sxb==eft.sx&&shell->ax==eft.ax&&std::abs(shell->ay-ay)<=1,where+": shell sx/ax/ay differ from WpnEffTab[wno][3] and the atan2f yaw");
        check(near(shell->lox,ops.x)&&near(shell->loy,ops.y)&&near(shell->loz,ops.z)&&shell->lkono==int(v1)&&shell->link_kind==1&&shell->link_index==v4,where+": shell not linked to the ene work owP[jno] with ops");
        check(puff->id==eft2.id&&puff->flg==eft2.flg&&!(puff->flg&0x80u)&&puff->link_kind==-1,where+": smoke [2] not the unlinked WpnEffTab[wno][2] record");
        check(near(puff->px,smoke.x)&&near(puff->py,smoke.y)&&near(puff->pz,smoke.z)&&near(puff->lox,ops2.x)&&near(puff->loz,ops2.z),where+": smoke position differs from owP[jno]*(cps + sz)");
        check(near(WpnEffTab[v3][2].px,smoke.x),where+": njCalcPoint did not write WpnEffTab[wno][2].px back (pwksub.c:3330)");
        check(WpnEffTab[v3][3].flg==0x40601u&&WpnEffTab[v3][3].flr_no==short(yk)&&WpnEffTab[v3][3].ay==short(shell->ay),where+": WpnEffTab[wno][3] not written like pwksub.c:3272-3305");
        std::cout<<where<<": shell slot "<<added[0]<<" ay "<<shell->ay<<" (expected "<<ay<<"), smoke slot "<<added[1]<<".\n";
        break;}
}
}

int main(int argc,char** argv){try{
    if(argc<2)return 0;const std::filesystem::path data=argv[1];
    _Make_SinTable(); // main.c/runtime.cpp: Ninja-Sinustabelle vor jeder Matrixrechnung (sonst Nullrotationen im owP)
    // --- 0x6e bhHEffectSet ---------------------------------------------------------------
    {std::vector<HEffect> h;heffect_room(data,"RM_3130",{2642},&h);
     // Feste Werte der Datenstelle: type 0, (78.90, 19.00, 20.00), ay -90 Grad = -16384, Effekt 398.
     check(h[0].effno==398&&h[0].type==0&&h[0].px==78.9f&&h[0].py==19.0f&&h[0].pz==20.0f&&h[0].ax==0&&h[0].ay==-16384,"RM_3130 @2642 record decodes differently");
     // Der gebundene Satz laeuft durch die portierte Prozedur effsub4.c:2722 bhEff_Sub398: Zweig type 0
     // erzeugt im ersten Pass zehn weitere Saetze id 398 type 1..10 (:2930-2942).
     Fixture f(data,"RM_3130");const auto cmd=f.command(2642,22,0x6e);const unsigned slot=f.first_free();check(f.bind(0x6e,2642,cmd),"RM_3130 0x6e not bound");
     f.world.set_simulation_flags(~0u);f.world.tick();
     const auto* s=f.world.effect_state(slot);check(s&&s->id==398&&s->mode0==1&&s->tex_id==30&&s->sxb==5.0f,"bhEff_Sub398 mode 0 did not run on the bound record");
     std::set<unsigned> types;for(unsigned i=0;const auto* e=f.world.effect_state(i);++i)if((e->flg&1u)&&e->id==398)types.insert(e->type);
     check(types.size()==11&&*types.begin()==0&&*types.rbegin()==10,"bhEff_Sub398 type 0 did not spawn types 1..10");
     std::cout<<"RM_3130 @2642: bhSetEffectEvt slot "<<slot<<", bhEff_Sub398 spawned "<<types.size()-1<<" children.\n";}
    {std::vector<HEffect> h;heffect_room(data,"RM_1090",{3228,3250},&h);
     // ay 180 Grad: 180 * 182.04445f = 32768 bleibt als int erhalten (O_WRK.ay ist int), nicht -32768 wie bei short.
     check(h[0].ay==32768&&h[1].ay==32768&&h[0].effno==113&&h[0].type==1&&h[0].pz==-5.0f,"RM_1090 records decode differently");}
    {std::vector<HEffect> h;heffect_room(data,"RM_0030",{3316},&h);
     // ax +10 / ay -10 Grad: Abschneiden zur Null (1820, -1820), nicht Abrunden (-1821); px negativ ueber Bit 1.
     check(h[0].ax==1820&&h[0].ay==-1820&&h[0].px==-10.0f,"RM_0030 record decodes differently");}
    heffect_room(data,"RM_0070",{1774});heffect_room(data,"RM_3100",{2636});heffect_room(data,"RM_3140",{1882});
    // Ueber die Event-VM: RM_1090 Ereignis 12 erreicht nach bhFrameWait 10 beide 0x6e (events.cpp world(t,op,22)).
    {Fixture f(data,"RM_1090");Inventory inventory;MessageDialog dialog;EventVm vm(f.bytes,inventory,dialog);
     vm.binding=[&](const EventCommand& c){return f.world.bind(c);};
     const auto before=f.active();vm.start(1,12);
     for(unsigned t=0;t<40&&f.added(before).size()<2;++t)vm.tick();
     const auto added=f.added(before);check(added.size()==2,"RM_1090 event 12 did not bind both 0x6e records");
     for(const auto& c:vm.pending_bindings)check(c.opcode!=0x6e,"0x6e left as pending binding");
     const auto h0=decode_heffect(f.command(3228,22,0x6e)),h1=decode_heffect(f.command(3250,22,0x6e));
     expect_evt(*f.world.effect_state(added[0]),h0,"RM_1090 VM @3228");expect_evt(*f.world.effect_state(added[1]),h1,"RM_1090 VM @3250");
     std::cout<<"RM_1090 event 12: the event VM binds both bhHEffectSet records (bhEff113).\n";}

    // --- 0x4a bhYakkyouSet ---------------------------------------------------------------
    // RM_0030 Ereignis 5 @3246: v0 1, v4 1 (id 91), v1 18 (jno), v2 0, v3 4 (wno), hand-0-Zweig.
    {Fixture f(data,"RM_0030");const auto cmd=f.command(3246,8,0x4a);
     check(cmd[1]==1&&cmd[2]==1&&cmd[3]==18&&cmd[4]==0&&cmd[5]==4,"RM_0030 @3246 record differs");
     yakkyou_check(f,3246,cmd,false,"RM_0030 @3246");
     // Diagnoseaufstellung: der zur Raumladezeit festgeschriebene owP-Baum dieses Charakters (id 91) hat
     // eine Null-3x3 (ay waere trivial 0). Mit gesetzten owP[18]/owP[0] werden njCalcPoint, atan2f-Gierwinkel
     // und der Rauchpunkt an einer echten Drehung geprueft; die Befehlsbytes bleiben die des Raumes.
     Fixture g(data,"RM_0030");yakkyou_check(g,3246,cmd,true,"RM_0030 @3246 (diagnostic owP)");}
    // RM_2050 Ereignis 14 @4014: v4 1, jno 22, hand 1, wno 8 -- gespiegelter Zweig pwksub.c:3290-3299.
    {Fixture f(data,"RM_2050");const auto cmd=f.command(4014,8,0x4a);
     check(cmd[1]==1&&cmd[2]==1&&cmd[3]==22&&cmd[4]==1&&cmd[5]==8,"RM_2050 @4014 record differs");
     yakkyou_check(f,4014,cmd,true,"RM_2050 @4014 (diagnostic owP, hand 1)");}
    // RM_6060 Ereignis 11 @6082/@6090: wno 8, hand 0 und hand 1 (Doppelpistole). pwksub.c:3248-3266
    // zaehlt je Hand eine Funktionsstatik hoch und laesst jeden ungeraden Stand aus: von zwei gleichen
    // Aufrufen erzeugt genau einer eine Huelse.
    {Fixture f(data,"RM_6060");
     for(const std::size_t offset:{std::size_t(6082),std::size_t(6090)}){const auto cmd=f.command(offset,8,0x4a);check(cmd[1]==1&&cmd[5]==8,"RM_6060 record differs");
        unsigned shells=0;for(unsigned k=0;k<2;++k){const auto before=f.active();check(f.bind(0x4a,offset,cmd),"RM_6060 0x4a not bound");
            for(const auto i:f.added(before))if(f.world.effect_state(i)->id==0)++shells;}
        check(shells==1,"wno 8: hr/hl alternation of pwksub.c:3248 not applied (hand "+std::to_string(cmd[4])+")");}
     std::cout<<"RM_6060 @6082/@6090: wno 8 hand 0/1 alternate like the static hr/hl.\n";}
    // Diagnoseaufstellung (geaenderte Bytes, keine Raumdaten): v0 2 faellt durch den switch und tut nichts.
    {Fixture f(data,"RM_0030");auto cmd=f.command(3246,8,0x4a);cmd[1]=2;const auto before=f.active();const unsigned yk=f.world.effect_counter();
     check(f.bind(0x4a,3246,cmd)&&f.added(before).empty()&&f.world.effect_counter()==yk,"bhYakkyouSet v0 2 must do nothing (event.c switch has only 0/1)");}
    std::cout<<"script effect opcodes 0x6e/0x4a: original fields reproduced.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
