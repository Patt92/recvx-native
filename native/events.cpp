#include "events.h"
#include "binary_view.h"
#include "room.h"
#include "ninja_math.h"
#include "inspection.h"
#include "core_random.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cstdio>
namespace recvx {
// Die sieben Event-Inventarmutationen (bhPlItemLost, bhItemGetGet,
// bhPlItemLostEx, bhArmsItemSet, bhItemGetGetEx, bhPlItemChangeM,
// bhPlItemTamaSet) laufen auf Inventory::script_* (D06). Die frühere
// generierte Brücke event_inventory.inc bleibt Quarantäne.
EventVm::EventVm(const std::vector<std::uint8_t>& room,Inventory& inv,MessageDialog& dialog):room_(room),inventory_(inv),dialog_(dialog){
    BinaryView v(room_);const auto info=inspect_room(room_);base_=info.event_offset;if(!base_)throw std::runtime_error("room event section missing");
    // EVT_WORK: scd0, scd1, then the Event_init offset table up to scd0. Rooms
    // without room events have only the two script offsets (header 8).
    const auto header=v.u32(base_);if(header<8||header%4||header>4096)throw std::runtime_error("invalid event table");
    v.require(base_,header);scd0_=v.relative(base_,header,1);scd1_=v.relative(base_,v.u32(base_+4),1);
    for(std::size_t p=8;p<header;p+=4)scripts_.push_back(v.relative(base_,v.u32(base_+p),1));
    messages_=v.u32(info.room_offset+0x38);v.require(messages_,4);
    {const auto posp=v.u32(info.room_offset+0x24),pos_n=v.u32(info.room_offset+0xa4);if(pos_n>4096)throw std::runtime_error("too many room positions");if(pos_n)v.require(posp,pos_n*16);for(unsigned i=0;i<pos_n;++i)positions_.push_back({v.f32(posp+i*16),v.f32(posp+i*16+4),v.f32(posp+i*16+8)});}
    const auto count=v.u32(info.room_offset+0x9c),offset=v.u32(info.room_offset+0x1c);if(count>4096)throw std::runtime_error("too many room interactions");v.require(offset,count*36);
    for(unsigned i=0;i<count;++i){const auto p=offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};for(unsigned j=0;j<4;++j)a.parameters[j]=v.u8(p+32+j);interactions.push_back(a);}
    const auto floor_count=v.u32(info.room_offset+0xa0),floor_offset=v.u32(info.room_offset+0x20);
    if(floor_count>4096)throw std::runtime_error("too many floor triggers");v.require(floor_offset,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};for(unsigned j=0;j<4;++j)a.parameters[j]=v.u8(p+32+j);floors_.push_back(a);}
    const auto item_count=v.u32(info.room_offset+0x90),item_offset=v.u32(info.room_offset+16);if(item_count>4096)throw std::runtime_error("too many room items");v.require(item_offset,item_count*36);for(unsigned i=0;i<item_count;++i){room_items_.push_back(v.u16(item_offset+i*36+4));room_item_flags_.push_back(v.u32(item_offset+i*36));}
    init_item_pool();
    for(unsigned level=0;level<tasks_.size();++level)tasks_[level].level=level;
    values[23]=inventory_.game_mode;values[24]=inventory_.player;flags[6][0]=0xffffffff;flags[13][0]=1;flags[14][0]=0x40000000;if(inventory_.side_pack)flags[11][0]|=0x8000000;
}
// bhCk/bhSet define matrices 1..16; no original storage is defined for 0.
// Keep undefined selectors visible instead of inventing a scratch flag bank.
bool EventVm::flag(unsigned matrix,unsigned index)const{if(!matrix||matrix>=flags.size())throw std::runtime_error("event flag outside matrix");index&=0x3ff;return bool(flags[matrix][index/32]&(0x80000000u>>(index%32)));}
void EventVm::set_flag(unsigned matrix,unsigned index,unsigned mode){if(!matrix||matrix>=flags.size()||mode>5)throw std::runtime_error("invalid event flag operation");index&=0x3ff;auto mask=(mode<3?0x80000000u:0xffffffffu)>>(index%32);auto& word=flags[matrix][index/32];switch(mode%3){case 0:word|=mask;break;case 1:word&=~mask;break;case 2:word^=mask;break;}if(matrix==11)inventory_.side_pack=bool(flags[11][0]&0x8000000);}
void EventVm::sync_player_stflg(){flags[14][0]=(flags[14][0]&~0x280000u)|inventory_.player_stflg();}
void EventVm::start(unsigned level,unsigned event){
    // Event_init (event.c:12964-12965): data = evtp + ((int*)evtp)[evt_id + 2], ohne Pruefung gegen die
    // Tabellenlaenge. Ein Index hinter der Tabelle liest das naechste u32 der Ereignisdaten als Offset
    // (RM_2040 scd1 @0x9bc Event_exec 3/39 bei 34 Eintraegen). Nur der Lesebereich bleibt geprueft.
    std::size_t pc;
    if(event<scripts_.size())pc=scripts_[event];
    else{const BinaryView v(room_);const auto entry=base_+8+std::size_t(event)*4;v.require(entry,4);pc=v.relative(base_,v.u32(entry),1);}
    if(level>=16){level=0;while(level<15&&tasks_[level].active)++level;}
    if(task_started)task_started(level);
    auto& task=tasks_[level];
    // Event_init (event.c:12956): only status, mode0/1, data, loop and ips/ian reset.
    task.active=true;task.blocked=false;task.event=event;task.pc=pc;task.loop=-1;task.delay=0;dispatched.push_back(event);
}
void EventVm::init_item_pool(){
    item_pool_flg_={};item_pool_id_={};
    // room.c:141-147 bhSetItmMdl -> bhSetItem: opp->flg = itp->flg, opp->id = itp->id fuer i < rom->itm_n.
    for(std::size_t i=0;i<room_items_.size()&&i<item_pool_flg_.size();++i){item_pool_flg_[i]=room_item_flags_[i];item_pool_id_[i]=room_items_[i];}
}
void EventVm::inherit_progress(const EventVm& previous){
    // sys->itwp ueberlebt den Raumwechsel; nur die Plaetze dieses Raums schreibt bhSetItem neu.
    for(std::size_t i=room_items_.size();i<item_pool_flg_.size();++i){item_pool_flg_[i]=previous.item_pool_flg_[i];item_pool_id_[i]=previous.item_pool_id_[i];}
    // Original global event/key/item/map/game flags outlive a ROM_WORK load.
    for(unsigned matrix:{1u,2u,3u,7u,8u,9u,11u,12u,15u,16u})flags[matrix]=previous.flags[matrix];
    // bhSysCallRoomRead -> bhInitReadRDT -> bhInitRoomChangeSystem (system.c:1629, room.c:32,
    // system.c:185): sys->gm_flg &= 0x9B8C00CB. Raumgebundene Kamerabits (0x800 bhCheckCut(1),
    // 0x1000/0x10000 Festkamera, 0x20000 bhFixEventCamPly, 0x10, 0x200000) enden mit dem Raum;
    // sonst haelt ein im Vorraum gesetztes 0x20000 jede Untersuchungskamera fest (RM_1130 -> RM_1080).
    flags[11][0]&=room_change_gm_mask;
    // system.c:191 bhInitRoomChangeSystem: sys->cb_flg &= 0xAF8000BB (Tuerbit 0x1 faellt erst in
    // bhStartDoorDemo Fall 1, room.c:1003; skriptgesetzte 0x1000000/0x8000000/0x20000000 ueberleben).
    flags[10][0]=previous.flags[10][0]&0xAF8000BBu;
    // player.c:862 bhInitRoomChangePlayer: plp->stflg &= 0x78280000 (Mottenei-Marken, Gift, 0x40000000).
    flags[14][0]=previous.flags[14][0]&0x78280000u;
    parked_wpnr_no=previous.parked_wpnr_no;
    // bhInitEvent resets status/wpnl_no only; the global task counters survive room loads.
    for(unsigned i=0;i<tasks_.size();++i){
        tasks_[i].loop_counts=previous.tasks_[i].loop_counts;
        tasks_[i].cnt2=previous.tasks_[i].cnt2;tasks_[i].cnt3=previous.tasks_[i].cnt3;
        tasks_[i].addpx=previous.tasks_[i].addpx;
    }
    values[7]=previous.values[7];values[18]=previous.values[18];values[25]=previous.values[25];values[22]=inventory_.slots()[0];
    evt_posno=previous.evt_posno; // sys->evt_posno lebt im SYS-Bereich ueber das Raumladen hinaus
    values[21]=previous.values[21]; // sys->rcase lebt ueber das Raumladen hinaus (bhRoomCaseNo 0x37, bhSv 21; system.c:1623 waehlt damit die naechste RDX)
    // sys->evt_tmd/evt_tim/evt_tdg/evt_fcd/evt_fcdct (bhSetEventTimer, system.c:2437) liegen im SYS-Bereich:
    // bhInitRoomChangeSystem (system.c:178) setzt sie nicht zurueck, nur bhInitGameOver (gameover.c:114)
    // und der Flagabbruch in bhControlVSync (sync.c:73) loeschen sie. Die Selbstzerstoerung (RM_4040 scd1
    // 0xcb Modus 1) laeuft ueber die Tuer in RM_3230/RM_3091 weiter.
    event_deadline_mode=previous.event_deadline_mode;event_deadline_ticks=previous.event_deadline_ticks;event_deadline_danger=previous.event_deadline_danger;
    event_deadline_voice_tick=previous.event_deadline_voice_tick;event_deadline_voice_count=previous.event_deadline_voice_count;
}
void EventVm::initialize(){
    check_sub_pack();
    for(auto& task:tasks_){task.active=false;task.blocked=false;task.wpnl_no=0;}
    if(initialize_enemies)initialize_enemies();
    current_task_=0;ifel_=0;event_timer=0;
    run(tasks_[current_task_],true,scd0_);
    flags[6][0]|=0x10;schedule();flags[6][0]&=~0x10u;
}
void EventVm::schedule(){
    if(!(flags[6][0]&0x10u))return;
    bool timed=false;
    for(unsigned level=0;level<tasks_.size();++level){
        current_task_=level;auto& task=tasks_[level];
        if(task.active){if(!timed){++event_timer;timed=true;}run(task);}
    }
}
// Common_controll (event.c:9963-12710) advances bhScePtr itself, command by
// command; bhCommonCtr (:9951) has consumed the 0x69 byte before. The lengths
// below are whole instructions (0x69 included), read off every original case
// (line = case label in event.c):
//   0 :12706 (++)                      2/3/4 :12167/12195/12223 (++ ++)
//   5/6 :11996/12005 (++ 3x++ ++)      7 :12034 (++ 3x +=2)     8 :12099 (++ 3x +=2)
//   9 :12136 (++ ++ ++ +=2)            10 :12452 (++, Achse 0..2: +=2 +=2)
//   11 :12068 (++ ++ ++ +=2)           12/13 :12019/12026 (++ 3x++ ++)
//   14/21 :12572/12583 (++ ++ ++)      15/16 :12594/12628 (++ ++ ++ +=2)
//   17/18/19 :12275/12310/12345 (++ ++) 22 :12403 (++, Achse 0..2: +=2 +=2)
//   23 :11971 (++ +=2)                 24 :11299 (++ ++, Selektor 0..3 je 5x++)
//   25 :10880 (++ ++, Selektor 0/1 je 5x++)
//   26 :10724 (++ ++ ++ 3x +=2)        27 :10764 (++ ++ ++ ++ ++ +=2)
//   28..33 :10384/10571/10472/10521/10644/10684 (++ ++)
//   34 :11632 (++ ++, Selektor 0..3: 5x++ +=2)
//   35 :11100 (++ ++, Selektor 0: 5x++; Selektor 1: ++ ++ ++ ++ +=2)
//   36 :12661 (++)                     38 :12667 (++ ++ ++ 3x +=2)
//   40/41/46 :10370/10346/10358 (++ +=2)  42 :10324 (++ ++ ++)
//   43 :10057 (++ ++ ++ ++)            44/45 :10184/10254 (++ ++ ++ ++)
//   47 :10053 (++)                     48 :10799 (++ +=2)       49 :10825 (++ ++ ++)
//   50 :10048 (++ +=2)                 51 :9997 (++ ++ ++ +=2)  52 :10030 (++ +=2)
//   53 :9967 (++ +=2)                  132 :12495 (++ ++ ++ ++ ++)
//   140 :12516 (++ ++ ++ +=2 +=2)      141 :12561 (++ ++ ++ ++ ++)
// A selector outside the original sub-switch (24/25/34/35, axis of 10/22) leaves
// the pointer on that byte: the next instruction starts there, as in the original.
std::size_t EventVm::common_controll_length(unsigned command,unsigned byte2,unsigned byte3){
    switch(command){
    case 0:case 36:case 47:return 2;
    case 2:case 3:case 4:case 17:case 18:case 19:case 28:case 29:case 30:case 31:case 32:case 33:return 3;
    case 14:case 21:case 23:case 40:case 41:case 42:case 46:case 48:case 49:case 50:case 52:case 53:return 4;
    case 43:case 44:case 45:return 5;
    case 5:case 6:case 9:case 11:case 12:case 13:case 15:case 16:case 51:case 132:case 141:return 6;
    case 10:case 22:return byte2<=2?6:2;
    case 7:case 8:case 27:case 140:return 8;
    case 26:case 38:return 10;
    case 24:return byte3<=3?8:3;
    case 25:return byte3<=1?8:3;
    case 34:return byte3<=3?10:3;
    case 35:return byte3==0?8:byte3==1?9:3;
    default:return 0;
    }
}
// Obj_controll (event.c:9719-9744) behind bhObjCtr (:9709, consumes 0x66, always returns 1):
//   20 :9727 (++ +=2: px/pz += bhEtask[v0].work->mlwP->objP->pos[0]/[2])   1 :9737 (mode1 = 0; ++)
//   default :9742 return without advancing: bhScePtr stays on the command byte, which the
//   scheduler loop (event.c:13047 while (bhScenarioJmpT[*bhScePtr]() != 0)) runs as the next opcode.
// In all 205 room scripts only 1, 20 and once 12 (RM_3020 Ereignis 6 @040e -> 0x0c bhFlrAtariSet) occur.
std::size_t EventVm::obj_controll_length(unsigned command){
    switch(command){
    case 1:return 2;
    case 20:return 4;
    default:return 1;
    }
}
void EventVm::world(Task& task,std::size_t& cursor,unsigned op,std::size_t length){
    BinaryView v(room_);v.require(cursor,length);EventCommand c{op,task.level,task.event,cursor-base_,{room_.begin()+cursor,room_.begin()+cursor+length}};
    c.loop_remaining=task.cnt2;c.loop_total=task.cnt3;
    c.simulation_flags=flags[6][0];
    if((!binding||!binding(c))&&reported_.insert({op,cursor}).second)pending_bindings.push_back(c);
    cursor+=length;
}
int EventVm::instruction(Task& t,std::size_t& cursor){
    BinaryView v(room_);const auto p=cursor;const unsigned op=v.u8(p);auto advance=[&](std::size_t n){v.require(p,n);cursor+=n;return 1;};
    /* Diagnose: RECVX_EVENT_TRACE=1 protokolliert jede ausgefuehrte Skriptanweisung (Event, Ebene, Offset, Bytes). */
    if(static const bool trace=std::getenv("RECVX_EVENT_TRACE")!=nullptr;trace){std::fprintf(stderr,"EVTRACE ev=%u lvl=%u off=%zu op=%02x:",t.event,t.level,p-base_,op);for(std::size_t i=1;i<10&&p+i<room_.size();++i)std::fprintf(stderr," %02x",v.u8(p+i));std::fputc('\n',stderr);}
    switch(op){
    // event.c:388 bhEnd: bhIfelFlg = 0.
    case 0:ifel_=0;return 0;
    // event.c:395 bhIfelCk: G_Sp = Ziel + 4 Bytes (unsigned int*, G_Sp++), bhIfelFlg++.
    // G_Sp ist ein einzelner globaler Zeiger, kein Stapel: ein inneres IF ueberschreibt ihn.
    case 1:gsp_=std::int64_t(v.relative(p+2,v.u8(p+1),1))+4;++ifel_;return advance(2);
    // event.c:413 bhElseCk / :425 bhEndif: G_Sp--, bhIfelFlg--; ohne offenes IF geht
    // der Zaehler ins Minus.
    case 2:gsp_-=4;--ifel_;cursor=v.relative(p,v.u8(p+1),1);return 1;
    case 3:gsp_-=4;--ifel_;return advance(2);
    case 4:{const unsigned matrix=v.u8(p+1),bit=v.u16(p+2),extra=v.u8(p+4),mode=v.u8(p+5);advance(6);if(matrix==10&&(((bit&0xffu)==22&&values[16]!=extra)||((bit&0xffu)==23&&values[20]!=extra)))return 0;return int(mode)^int(flag(matrix,bit&0x3ff));}
    case 5:set_flag(v.u8(p+1),v.u16(p+2)&0x3ff,v.u8(p+5));return advance(6);
    // event.c:701 bhCmpB key 17 reads bhEtask->wpnl_no, i.e. task 0, whichever task compares.
    case 6:{const unsigned key=v.u8(p+1),mode=v.u8(p+2),operand=v.u8(p+3);advance(4);const unsigned value=key==17?tasks_[0].wpnl_no:values[key];switch(mode){case 0:return value==operand;case 1:return value>operand;case 2:return value>=operand;case 3:return value<operand;case 4:return value<=operand;case 5:return value!=operand;default:throw std::runtime_error("unsupported event byte comparison");}}
    // bhCmpW: enemy index, field selector, comparison, unsigned 16-bit operand.
    // The selected work value is signed (in particular scripted enemy HP).
    case 7:{v.require(p,6);const unsigned enemy=v.u8(p+1),field=v.u8(p+2),mode=v.u8(p+3);const int operand=v.u16(p+4);int value=0;
        switch(field){case 5:value=inventory_.hp;break;case 7:value=dialog_.active()?dialog_.selection:values[7];break;case 9:value=inventory_.play_time;break;case 10:{if(!enemy_health)throw std::runtime_error("word HP comparison requires native enemy binding");const auto hp=enemy_health(enemy);if(!hp)throw std::runtime_error("word HP comparison enemy unavailable");value=*hp;break;}default:throw std::runtime_error("unsupported event word field "+std::to_string(field));}
        int result=0;switch(mode){case 0:result=value==operand;break;case 1:result=value>operand;break;case 2:result=value>=operand;break;case 3:result=value<operand;break;case 4:result=value<=operand;break;case 5:result=value!=operand;break;default:throw std::runtime_error("unsupported event word comparison");}advance(6);return result;}
    case 9:{v.require(p,4);const unsigned field=v.u8(p+1),operand=v.u16(p+2);if(field==5)inventory_.hp=operand;else if(field==9)inventory_.play_time=operand;else if(field>=11&&field<=15){if(!set_enemy_health)throw std::runtime_error("word HP assignment requires native enemy binding");set_enemy_health(field-11,-int(operand));}return advance(4);}
    case 0x37:values[21]=v.u8(p+1);return advance(2); // bhRoomCaseNo
    case 0x57:if(!v.u8(p+1))flags[11][0]|=0x20000;else flags[11][0]&=~0x20000u;return advance(2); // bhFixEventCamPly
    case 8:{const auto key=v.u8(p+1),value=v.u8(p+2);values[key]=value;if(key==18){inventory_.wpnr_no=value;inventory_.partner_weapon.at(inventory_.player)=std::uint8_t(value);} /* event.c:827-830 plp->wpnr_no/ply_wno, ohne Neuladen */if(key==22){if(value>=12)throw std::runtime_error("equipped inventory index outside slots");inventory_.slots()[0]=value;}return advance(4);}
    // bhDieCk masks the flag index with 0x3ff before touching sys->ed_flg.
    case 0xd:{if(enemy_dead&&enemy_dead(v.u8(p+1)))set_flag(3,v.u16(p+2)&0x3ff,0);advance(4);return 1;}
    // bhEneSetCk: an enemy whose ed_flg bit is already set stays deactivated.
    case 0x22:{const unsigned enemy=v.u8(p+1),bit=v.u16(p+2)&0x1ff;advance(4);if(flag(3,bit)){if(!enemy_disable)throw std::runtime_error("enemy set check requires native world binding");enemy_disable(enemy);}return 1;}
    // bhCamCheck: only after an original skip (sp_flg 0x40) does an active
    // event camera v0 (and key v1 when v2) make the condition false.
    case 0x62:{const unsigned camera=v.u8(p+1),key=v.u8(p+2),match_key=v.u8(p+3);advance(6);if(!(flags[6][0]&0x40))return 1;
        if(flags[5][0]&1){unsigned active=0,active_key=0;if(!event_camera)throw std::runtime_error("camera check requires native world binding");if(event_camera(active,active_key)&&active==camera&&(!match_key||active_key==key))return 0;}return 1;}
    // bhCamInfoSet toggles CUT_WORK.flg bit 0 of camera-switch group v1.
    // event.c:994 bhCamSet2: case 0 sets st_flg 0x1, gm_flg 0x1000 and sys->fixcno = v1, then
    // bhSetFixedCut (cut.c:448) clears gm_flg 0x1000 at once; game.c:274 must not see it later.
    case 0x2a:{const auto mode=v.u8(p+1);if(!mode){flags[5][0]|=1;fixcno=v.u8(p+2);}world(t,cursor,op,4);if(!mode)flags[11][0]&=~0x1000u;else if(mode==1){flags[5][0]&=~1u;flags[11][0]&=~0x1000u;}return 1;}
    case 0x39:world(t,cursor,op,4);return 1;
    // event.c:3488 bhLookGsetPlStart / :3539 bhLookGsetPlStop: Blickpunkt des Spielerkopfes.
    case 0x40:world(t,cursor,op,8);return 1;
    case 0x41:world(t,cursor,op,2);return 1;
    // Befehle mit bekannter Laenge, deren Wirkung nativ keinen Besitzer hat. Sie werden
    // korrekt uebersprungen und als offene Bindung gemeldet, statt den Raum anzuhalten:
    // ein unbekannter Befehl wirft unten und beendet den Lauf.
    //   0x6F bhObjLinkSetObjPly (event.c:5263) bindet RoomWorld::bind (Spielerwurzel am Objekt).
    //   0x82 bhSetDebugLoopEx (:2421), 0x97 bhEffectKokuenSet (:1986), 0xAA bhEffUVSet2 (:7159)
    //        und 0xAF bhPlyHandChange (:7378) bindet RoomWorld::bind; das Feuerzeug-Parken
    //        aus 0xAF fuehrt der Fall unten. Stand 2026-10-21: keiner dieser Befehle ist
    //        mehr ohne Besitzer; die Liste bleibt als Lesehilfe.
    case 0x6f:world(t,cursor,op,12);return 1;
    case 0x82:world(t,cursor,op,4);return 1;
    case 0x97:world(t,cursor,op,8);return 1;
    case 0xaa:world(t,cursor,op,10);return 1;
    // event.c:7416-7465: Feuerzeug parken/zurueckgeben ueber bhEtask[1].wpnr_no (Spielerwert, nicht Task t).
    // Die owP[7..9].flg 0x2 folgen plp->wpnr_no == 1 in jedem Spielerpass (player.c:1486).
    case 0xaf:{const bool take=v.u8(p+3)==0;world(t,cursor,op,4);
        if(take){if(inventory_.wpnr_no==1){parked_wpnr_no=1;inventory_.wpnr_no=0;}}
        else if(parked_wpnr_no==1){parked_wpnr_no=0;inventory_.wpnr_no=1;}
        return 1;}
    // event.c:9049 bhRankingCall: ss_flg 0x20000 nur innerhalb der Routine; njFogDisable/njSetBackColor(0)/
    // bhReleaseMainTexture/Ps2ClearOT/bhDrawScreenFade sind die Bildpraesentation des Taskwechsels;
    // tk_flg = 0x702040 (Pad, Ending = ranking.c ControlRanking, Monitor, SndMonitor, ScreenSaver),
    // ts_flg = 0, typ_md0 = 0, typ_flg = 0, ssv_tim = 0, memp = mempb. Die Taskmaske traegt der Host.
    case 0xd3:flags[12][0]=0;ranking_requested=true;return advance(2);
    // event.c:4110 bhEffectSandSet: v0 Raumgegner, v1 Vorzeichenbits, v2 Lageart, drei u16
    // Betraege, dann der Typ als ein Byte — bhScePtr rueckt danach um zwei (12 Byte gesamt).
    case 0x98:world(t,cursor,op,12);return 1;
    // event.c:8577 bhEffectSandSetMatsumoto: drei Bytes nach dem Befehl.
    case 0xc3:world(t,cursor,op,4);return 1;
    // Original Event_T_timer: one increment per scheduler pass with active work.
    case 0x60:{const auto limit=v.u16(p+4);advance(6);return event_timer<limit;}
    case 0x61:event_timer=0;return advance(2);
    // bhEvtTimerSet: sys->evt_tmd is mode+1 even for an otherwise unused
    // mode.  The three defined modes also arm their original event flag and
    // fixed VSync-unit deadline in bhSetEventTimer (system.c).
    case 0xcb:{const unsigned mode=v.u8(p+1);event_deadline_mode=mode+1;
        switch(mode){
        case 0:set_flag(1,69,0);event_deadline_ticks=2700;event_deadline_danger=900;break;
        case 1:set_flag(1,67,0);event_deadline_ticks=18000;event_deadline_danger=3600;break;
        case 2:set_flag(1,70,0);event_deadline_ticks=18000;event_deadline_danger=3600;event_deadline_voice_tick=900;event_deadline_voice_count=0;break;
        }return advance(2);}
    case 0xd0:world(t,cursor,op,4);return 1;
    case 0x79:world(t,cursor,op,6);return 1;
    case 0x7a:world(t,cursor,op,2);return 1;
    case 0x78:world(t,cursor,op,14);return 1;
    // event.c:5454/5516 bhLightParameterCSet/Start, :7067/7119 bhFogParameterCSet/Start. Both Start
    // commands return 0 and end the pass inside their FOR loop.
    case 0x73:world(t,cursor,op,22);return 1;
    case 0x74:world(t,cursor,op,4);return 0;
    case 0xa8:world(t,cursor,op,10);return 1;
    case 0xa9:world(t,cursor,op,2);return 0;
    case 0xba:world(t,cursor,op,16);return 1;
    case 0xbb:world(t,cursor,op,2);return 0;
    case 0x80:case 0x7e:world(t,cursor,op,4);return 1;
    case 0x4f:world(t,cursor,op,6);return 1;
    case 0x88:case 0xd2:world(t,cursor,op,10);return 1;
    case 0x45:world(t,cursor,op,2);return 1;
    // bhObjCtr/Obj_controll: cases 1 and 20 act on the task work (RoomWorld::bind); the default branch has no
    // effect and consumes only the 0x66 byte, bhObjCtr still returns 1 (event.c:9709-9744).
    case 0x66:{const auto length=obj_controll_length(v.u8(p+1));if(length==1)return advance(1);world(t,cursor,op,length);return 1;}

    case 0xb9:world(t,cursor,op,10);return 1;
    // bhAreaSearchObj: X/Z minima inclusive, maxima exclusive; signed
    // magnitudes use separate masks. Byte 8 is padding, not the second mask.
    case 0x72:{v.require(p,14);if(!object_position)throw std::runtime_error("object area check requires native world binding");const unsigned kind=v.u8(p+2);if(kind>3)throw std::runtime_error("object area check kind outside original table");const auto position=object_position(kind,v.u8(p+1));if(!position)throw std::runtime_error("object area check entity unavailable");
        const auto first=v.u8(p+3),second=v.u8(p+9);const float x1=v.u16(p+4)/100.f*(first&1?-1:1),z1=v.u16(p+6)/100.f*(first&4?-1:1),x2=v.u16(p+10)/100.f*(second&1?-1:1),z2=v.u16(p+12)/100.f*(second&4?-1:1);advance(14);return x1<=position->x&&position->x<x2&&z1<=position->z&&position->z<z2;}
    // bhDieEventCk: compare the number of set ed_flg bits base..base+count-1.
    case 0x21:{const unsigned count=v.u8(p+1),base=v.u16(p+2),mode=v.u8(p+4),operand=v.u8(p+5);advance(6);unsigned dead=0;for(unsigned i=0;i<count;++i)dead+=flag(3,(base+i)&0x1ff);
        switch(mode){case 0:return dead==operand;case 1:return dead>operand;case 2:return dead>=operand;case 3:return dead<operand;case 4:return dead<=operand;case 5:return dead!=operand;default:return 0;}}
    case 0xb:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");if(v.u8(p+2))interactions[index].flags&=~1u;else interactions[index].flags|=1;return advance(4);}
    case 0xc:{const auto index=v.u8(p+1);if(index>=floors_.size())throw std::runtime_error("script floor outside room");if(v.u8(p+2))floors_[index].flags&=~1u;else floors_[index].flags|=1;world(t,cursor,op,4);return 1;}
    case 0x38:{const auto kind=v.u8(p+1),index=v.u8(p+2),half=v.u8(p+3);const auto target=unsigned(v.u16(p+4))*65536u+(half?32768u:0);if(!animation_frame)throw std::runtime_error("frame condition requires native actor binding");advance(6);return std::int32_t(animation_frame(kind,index))<std::int32_t(target);}
    case 0x1b:{const auto slot=v.u8(p+2);if(!audio_active)throw std::runtime_error("audio condition requires native mixer binding");advance(6);return audio_active(slot);}
    // event.c:1523 bhAdxTimeCk: Abspielzeit des ADX-Stroms gegen einen Grenzwert in
    // Hundertstelsekunden. Sechs Byte: Platz, u16-Grenze, Modus, Fuellbyte. Die Modi 0..3
    // liefern 0, WENN die Bedingung zutrifft (:1546-1588), die Modi 4 und 5 liefern 1
    // (:1590-1610) — in beiden Faellen ist das Ergebnis also dieselbe Tabelle wie bei
    // bhMovieTimeCk unten. Anders als dort hat das Original KEINE Vorpruefung auf einen
    // laufenden Strom, und der switch hat keinen default: bei einem Modus ueber 5 bleibt die
    // rohe Zeit stehen und gilt als wahr, sobald sie ungleich 0 ist (:1544).
    case 0x1e:{v.require(p,6);const unsigned limit=v.u16(p+2),mode=v.u8(p+4),slot=v.u8(p+1);
        if(!adx_time)throw std::runtime_error("ADX time condition requires native mixer binding");
        const unsigned time=adx_time(slot);advance(6);
        switch(mode){case 0:return time!=limit;case 1:return time<=limit;case 2:return time<limit;
                     case 3:return time>=limit;case 4:return time>limit;case 5:return time==limit;
                     default:return int(time);}}
    // bhMovieTimeCk compares centiseconds and returns the inverted condition.
    case 0xac:{v.require(p,6);const unsigned limit=v.u16(p+2),mode=v.u8(p+4);
        if(!movie_active||!movie_time)throw std::runtime_error("movie time condition requires native player binding");
        if(!movie_active())return advance(6);
        const unsigned time=movie_time();int result=0;switch(mode){case 0:result=time!=limit;break;case 1:result=time<=limit;break;case 2:result=time<limit;break;case 3:result=time>=limit;break;case 4:result=time>limit;break;case 5:result=time==limit;break;default:result=int(time);break;}advance(6);return result;}
    case 0x9b:if(!movie_active)throw std::runtime_error("movie condition requires native player binding");advance(6);return movie_active();
    case 0xe:{const unsigned bit=v.u16(p+2)&0x3ff,interaction=v.u8(p+4),mode=v.u8(p+5);advance(6);if(values[20]!=interaction)return 1;
        if(interaction>=interactions.size())throw std::runtime_error("pickup interaction "+std::to_string(interaction)+" outside room table of "+std::to_string(interactions.size()));
        // ic_flg wird mit (v2 & 0x1E0) >> 5 geloescht, it_flg mit 0x3E0 gesetzt (event.c:2131-2143).
        if(flags[10][0]&0x800){if(!flag(7,bit)){if(!mode)interactions[interaction].flags&=~1u;disable_item(t,op,p,interactions[interaction].parameters[0]);set_flag(7,bit,0);set_flag(9,bit&0x1ff,1);}}
        else if(!flag(7,bit))set_flag(9,bit,0);flags[10][0]&=~0x800u;return 1;}
    case 0x23:{const unsigned item=v.u8(p+1),bit=v.u16(p+2)&0x1ff /* event.c:2772: (v3 & 0x1FF) >> 5 */,interaction=v.u8(p+4),mode=v.u8(p+5);advance(6);
        // bhItmSetCk indexes rom->etcp[interaction] unchecked. Some original scripts name an entry beyond the table
        // (the PS2 would touch unrelated room memory); the ATR flag write is skipped and reported, the item part stays.
        if(interaction>=interactions.size()){pending_bindings.push_back(EventCommand{op,t.level,t.event,p-base_,{room_.begin()+p,room_.begin()+p+6}});if(flag(7,bit))disable_item(t,op,p,item);return 1;}
        if(flag(7,bit)){if(!mode)interactions[interaction].flags&=~1u;disable_item(t,op,p,item);}else interactions[interaction].flags|=1;return 1;}
    case 0xf:flags[10][0]&=~0x400u;advance(2);return 0;
    case 0x10:{const auto item=v.u8(p+1);advance(2);return bool(flags[10][0]&0x400)&&values[8]==item;}
    case 0x11:{const auto item=v.u8(p+1);advance(2);for(unsigned slot=1;slot<12;++slot){const auto packed=inventory_.slots()[slot];if(Inventory::id(packed)==item){if(item==72&&(packed>>16)!=72)flags[4][0]|=0x10000;return 1;}}return 0;}
    case 0x4d:{const auto mode=v.u8(p+1),operand=v.u8(p+2);std::uint32_t empty=0,wide=0;for(unsigned slot=2;slot<inventory_.capacity()+2;++slot){const auto packed=inventory_.slots()[slot];if(!packed)++empty;switch(Inventory::id(packed)){case 1:case 2:case 3:case 33:case 34:case 142:++wide;break;}}const auto available=empty-wide;advance(4);switch(mode){case 0:return available==operand;case 1:return available>operand;case 2:return available>=operand;case 3:return available<operand;case 4:return available<=operand;case 5:return available!=operand;default:return 1;}}
    case 0x8a:{const auto mode=v.u8(p+1);advance(2);return inventory_.infected==(mode==0);} // bhPlayerPoisonCk tests plp->stflg 0x200000
    // bhEnemyHpUp deliberately divides before multiplying (integer HP).
    case 0x99:{v.require(p,4);const unsigned index=v.u8(p+1),percent=v.u8(p+2);
        if(!enemy_health||!set_enemy_health)throw std::runtime_error("enemy HP increase requires native enemy binding");
        const auto hp=enemy_health(index);if(!hp)throw std::runtime_error("enemy HP increase enemy unavailable");
        set_enemy_health(index,*hp+(*hp/100)*int(percent));return advance(4);}
    case 0xb7:{const auto player=v.u8(p+1);if(player>=4)throw std::runtime_error("security deposit player outside original inventory");std::fill(inventory_.items.begin()+192,inventory_.items.begin()+256,0);for(unsigned slot=2;slot<16;++slot){inventory_.items[192+slot-2]=inventory_.items[player*16+slot];inventory_.items[player*16+slot]=0;}return advance(2);}
    case 0xb8:{auto items=inventory_.items;unsigned source=192;for(unsigned target=64;target<192&&source<208;++target){if(!items[source])break;if(!items[target]){items[target]=items[source];items[source++]=0;}}if(source<208&&items[source])throw std::runtime_error("original security transfer exceeds item-box capacity");inventory_.items=items;return advance(2);}
    case 0x12:{const auto mode=v.u8(p+1);advance(2);
        switch(mode){
        case 0:case 5:flags[10][0]=(flags[10][0]&~0x40u)|4;flags[5][0]|=4;break;
        case 1:flags[10][0]&=~4u;flags[5][0]&=~4u;flags[6][0]=0xffffffff;break;
        case 2:flags[10][0]&=~4u;flags[5][0]&=~4u;break;
        case 3:flags[10][0]|=0x44;flags[5][0]|=4;break;
        case 4:flags[10][0]&=~0x44u;flags[5][0]&=~4u;flags[6][0]=0xffffffff;break;
        default:throw std::runtime_error("invalid CineSet mode");}return 1;}
    // event.c:4942 bhRandamSet: single shared EE rand(), float scaling, then modulo, stored in
    // bhEtask->wpnl_no (task 0) for every caller; :6407 bhRandTest writes the same task's addpx.
    case 0x63:{const unsigned limit=v.u8(p+1);if(!limit)throw std::runtime_error("bhRandamSet modulus zero");const int scaled=int(100.0f*(float(recvx_core_random_next())/2147483648.0f));tasks_[0].wpnl_no=unsigned(scaled)%limit;return advance(2);}
    // event.c bhRandTest (opcode 0x8c): optional global srand(seed), then
    // one original rand() stored both as float bits and wpnl_no.
    case 0x8c:{const unsigned mode=v.u8(p+1),seed=(v.u16(p+2)<<16)|v.u16(p+4);advance(6);if(mode==0)recvx_core_random_seed(seed);tasks_[0].addpx=float(recvx_core_random_next())/2147483648.0f;std::memcpy(&tasks_[0].wpnl_no,&tasks_[0].addpx,sizeof tasks_[0].wpnl_no);return 1;}
    case 0x14:{const auto packed=v.u16(p+2);advance(4);start(packed&255,packed>>8);return 1;}
    case 0x1f:{const auto mode=v.u8(p+1),index=v.u8(p+2),freeze=v.u8(p+3);advance(4);if(!mode){show_dialog(read_message(room_,messages_,index),index);opening_hint_=t.event==16&&index==4;if(!freeze)flags[6][0]&=~7u;}else flags[6][0]|=7;return 1;}
    case 0x25:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");auto& a=interactions[index];a.attribute=v.u16(p+2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(p+4+i);a.type=v.u8(p+8);return advance(10);}
    case 0x7b:world(t,cursor,op,10);return 1;
    case 0x7c:{const auto index=v.u8(p+1);if(index>=floors_.size())throw std::runtime_error("script floor outside room");auto& a=floors_[index];a.attribute=v.u16(p+2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(p+4+i);a.type=v.u8(p+8);world(t,cursor,op,10);return 1;}
    case 0x26:{const auto weapon=v.u8(p+1);advance(2);return inventory_.weapon()==weapon;}
    // event.c:1285 bhArmsItemChange: plp->wpnr_no und ply_wno[ply_id], mn_mode0 3 (bhReadWeaponData); pip[0] bleibt.
    case 0x27:values[18]=v.u8(p+1);inventory_.wpnr_no=v.u8(p+1);inventory_.partner_weapon.at(inventory_.player)=v.u8(p+1);inventory_.request_weapon_reload();return advance(2);
    case 0x28:status_requested=true;flags[12][0]|=0x200;return advance(2);
    // event.c Inventarmutationen auf dem einzigen Bestandsbesitzer Inventory
    // (native/inventory.cpp, Vertrag work/native_port_inventory_contract.md).
    // Operandenlängen aus dem bhScePtr-Vorschub; Rückgabe 1 wie im Original.
    case 0x31:inventory_.script_item_lost(v.u8(p+1));values[22]=inventory_.slots()[0];return advance(2);            // bhPlItemLost
    case 0xb2:inventory_.script_item_give(v.u8(p+1),v.u8(p+2),v.u8(p+3));values[22]=inventory_.slots()[0];return advance(4); // bhItemGetGet
    case 0xbf:inventory_.script_item_lost_ex(v.u8(p+1),v.u8(p+2));values[22]=inventory_.slots()[0];return advance(4); // bhPlItemLostEx
    case 0xc1:inventory_.script_arms_set(v.u8(p+1));values[22]=inventory_.slots()[0];return advance(2);              // bhArmsItemSet
    case 0xc2:inventory_.script_item_give_ex(v.u8(p+1),v.u8(p+2),v.u8(p+3),v.u16(p+4));values[22]=inventory_.slots()[0];return advance(6); // bhItemGetGetEx
    case 0xc7:inventory_.script_item_change(v.u8(p+1),v.u8(p+2));return advance(4);                                 // bhPlItemChangeM
    case 0xc8:world(t,cursor,op,8);return 1; // event.c:8722 bhEffBakuDrmSet: Vorzeichenbyte + 3 u16/100, bhSetExplosionEffect am Weltbesitzer
    case 0xc9:inventory_.script_item_amount(v.u8(p+1),v.u16(p+2));return advance(4);                                // bhPlItemTamaSet

    case 0x5c:dialog_=MessageDialog{};flags[5][0]&=~0x200u;return advance(2);
    case 0x81:{const auto mode=v.u8(p+1);advance(2);return mode==unsigned(!menu_open);}
    case 0xbc:{const auto level=v.u8(p+1);if(level>=16)throw std::runtime_error("event kill outside task table");advance(2);tasks_[level].active=false;return 1;}
    case 0xf8:{ // bhSleep increments the same loop index as FOR/WHILE, then decrements once.
        v.require(p,4);if(t.loop+1>=int(t.loop_counts.size()))throw std::runtime_error("event SLEEP counter outside original table");
        t.loop_counts.at(++t.loop)=v.u16(p+2);++cursor;
        t.delay=--t.loop_counts.at(t.loop);
        if(!t.delay){cursor+=3;--t.loop;}return 0;}
    case 0xf9:t.delay=--t.loop_counts.at(t.loop);if(!t.delay){cursor+=3;--t.loop;}return 0;
    case 0xfe:++cursor;return 0;
    case 0xf4:++cursor;return 1;
    case 0xff:t.active=false;advance(2);return 0;
    case 0xfc:{
        if(t.loop+1>=int(t.loop_body.size()))throw std::runtime_error("event WHILE outside original stack");
        const auto body=v.relative(p,v.u8(p+1),1);++t.loop;
        t.loop_condition.at(t.loop)=p+2;t.loop_body.at(t.loop)=body;cursor=body;return 1;}
    case 0xfd:case 0xf3:{
        // bhEwhile saves bhScePtr in data; its condition may also change task state.
        t.pc=p;cursor=t.loop_condition.at(t.loop);
        if(instruction(t,cursor)!=0){cursor=t.loop_body.at(t.loop);return 1;}
        cursor=t.pc+1;--t.loop;return op==0xf3?1:0;}
    case 0xfa:{
        v.require(p,4);if(t.loop+1>=int(t.loop_counts.size()))throw std::runtime_error("event FOR counter outside original table");
        ++t.loop;t.cnt2=t.cnt3=t.loop_counts.at(t.loop)=std::uint32_t(std::int32_t(std::int16_t(v.u16(p+2))));
        t.loop_body.at(t.loop)=p+4;return advance(4);}
    case 0xfb:if(--t.loop_counts.at(t.loop)!=0){t.cnt2=t.loop_counts.at(t.loop);cursor=t.loop_body.at(t.loop);}else{advance(2);--t.loop;}return 1;
    // The binary structure is known, but these commands still need native
    // world/audio/cutscene bindings; keep them available for the next port step.
    case 0x13:{const auto mode=v.u8(p+1);if(mode==1)flags[5][0]&=~1u; /* event.c:966-967: st_flg 0x1 faellt vor bhCheckCut(1) */world(t,cursor,op,4);return 1;}
    case 0xad:case 0xd4:case 0x94:case 0xa:case 0x3d:case 0x3f:case 0x20:case 0x24:case 0x2f:case 0x35:case 0x3c:case 0x3e:case 0x43:case 0x4b:case 0x65:case 0x91:case 0x9a:case 0xa6:world(t,cursor,op,4);return 1;
    case 0x75:world(t,cursor,op,4);return 1;
    case 0x8b:world(t,cursor,op,12);return 1;
    case 0x8e:case 0x5a:case 0x1d:world(t,cursor,op,4);return 1;
    case 0x9f:case 0x50:world(t,cursor,op,2);return 1;
    case 0x6b:world(t,cursor,op,2);return 1; // bhDelYakkyou (including padding)
    case 0x6c:world(t,cursor,op,8);return 1;
    case 0x49:world(t,cursor,op,10);return 1;
    case 0xa2:world(t,cursor,op,4);return 1; // bhEvtLighterFireSet
    case 0x8d:flags[10][0]|=0x400000;com_number=v.u8(p+1);return advance(2); // bhEvtComSet: cb_flg 0x400000 starts bhSysCall's computer task
    case 0x4a:world(t,cursor,op,8);return 1;
    case 0x1c:world(t,cursor,op,6);return 1;
    case 0x6e:world(t,cursor,op,22);return 1;
    case 0xb0:world(t,cursor,op,22);return 1; // event.c:7472 bhHEffectSet2: sys->ef -> bhSetEffectTb(&sys->ef, NULL, NULL, v4), 22 Byte (RM_1050 Ereignis 12/18/24, RM_5001, RM_9110)
    case 0x6d:world(t,cursor,op,22);return 1; // event.c:5028 bhCyodanSet: bhSetEffParticle am Weltbesitzer
    // event.c:8307 bhCyodanSetEx (RM_0090 event 23, the laser): same Cyodan
    // family as 0x6d plus a second colour and a work/node link. 28 bytes is
    // measured against the data: RM_0090 event 23 holds two adjacent 0xc0
    // records exactly 28 bytes apart. RoomWorld::bind spawns bhSetEffParticleMk2
    // (id 302, PtclTbl) through the native particle owner.
    case 0xc0:world(t,cursor,op,28);return 1;
    case 0xab:world(t,cursor,op,6);return 1; // bhBGColorSet, including deferred application.
    case 0xd1:world(t,cursor,op,10);return 1;
    case 0x17:world(t,cursor,op,8);return 1;
    case 0x2d:world(t,cursor,op,2);return 1;
    // event.c:1827 bhMotionPauseSetPly: sys->sp_flg Bit 0x1, das Tor des Spielerpasses
    // (player.c:1365 bhControlPlayer). Dieselbe Bank, die die Event-VM selbst fuehrt.
    case 0x2e:if(v.u8(p+1)==1)flags[6][0]|=1u;else if(v.u8(p+1)==0)flags[6][0]&=~1u;return advance(2);
    case 0x30:world(t,cursor,op,4);return 1;
    case 0x48:world(t,cursor,op,6);return 1;
    case 0x86:world(t,cursor,op,16);return 1;
    case 0x4c:case 0x44:world(t,cursor,op,6);return 1;
    case 0x33:{v.require(p,8);DoorRequest request{v.u16(p+2),v.u8(p+4),v.u8(p+5),v.u8(p+6),v.u8(p+7),values[21]};if(!request_door(request,true))throw std::runtime_error("DoorCall destination unavailable");return advance(8);}
    case 0x32:case 0x34:case 0x51:case 0x52:case 0x53:case 0x54:case 0x59:case 0xa3:world(t,cursor,op,12);return 1;
    case 0x3b:world(t,cursor,op,6);return 1;
    case 0x36:world(t,cursor,op,6);return 1;
    case 0x6a:if(!v.u8(p+1))flags[11][0]|=0x40000000u;else flags[11][0]&=~0x40000000u;return advance(2);
    case 0x2b:world(t,cursor,op,2);return 1; // bhMotionPauseSet: length 2 (opcode + mode)
    case 0x16:case 0x18:case 0x1a:case 0x42:case 0x5b:case 0x5f:case 0x29:case 0x9c:case 0x9e:world(t,cursor,op,op==0x42?4:2);return 1;
    // bhFaceReSet: one enemy index; facial control is bound in RoomWorld.
    case 0x90:world(t,cursor,op,2);return 1;
    case 0xca:world(t,cursor,op,2);return 1; // event.c:8801 bhEffClearEvt (2 Bytes): bhClearEventEffect am Weltbesitzer (RoomWorld::clear_event_effects)
    case 0x8f:case 0x5e:world(t,cursor,op,4);return 1;
    case 0x95:case 0xa7:world(t,cursor,op,4);return 1; // bhBgmOn2/bhBgmOn2Ex
    case 0x96:world(t,cursor,op,2);return 1; // event.c:1965 bhEffectSensyaSet (operand is padding)
    case 0x92:world(t,cursor,op,2);return 1; // bhBGSeOff2
    case 0x93:world(t,cursor,op,2);return 1;            // bhBgmOff2
    case 0x77:world(t,cursor,op,6);return 1; // bhSoundVolumeSet
    case 0xb6:world(t,cursor,op,2);return 1; // bhRoomSoundCase -> SetRoomSoundCaseNo
    case 0x19:case 0xc4:world(t,cursor,op,8);return 1;
    case 0x64:{unsigned length=0;switch(v.u8(p+1)){case 7:length=8;break;case 12:case 13:length=6;break;case 0x80:case 0x34:case 0x8b:case 0:length=2;break;case 0x83:case 0x89:length=4;break;case 150:length=4;break;case 129:length=12;break;case 130:case 142:length=2;break;default:throw std::runtime_error("unimplemented Player_controll command "+std::to_string(v.u8(p+1)));}world(t,cursor,op,length);return 1;}
    case 0x67:{unsigned length=0;switch(v.u8(p+1)){case 0:case 0x80:case 0x8b:case 0x90:length=2;break;case 0x8f:length=6;break;case 0x92:case 0x93:length=8;break;case 0x94:case 0x95:length=4;break;default:throw std::runtime_error("unimplemented Sub_controll command "+std::to_string(v.u8(p+1)));}world(t,cursor,op,length);return 1;}
    case 0x15:world(t,cursor,op,4);return 1;
    // bhCyoutenHenkeiStart/StartEX: task work morph step inside the FOR clock
    // (shp_ct recomputed per pass). Return 0 ends the pass like the original.
    case 0x55:world(t,cursor,op,2);return 0;
    case 0x84:world(t,cursor,op,8);return 1; // bhCyoutenHenkeiSetEX
    case 0x85:world(t,cursor,op,4);return 0;
    case 0x7d:world(t,cursor,op,4);return 1; // bhMotionPosSetEnePly
    case 0x9d:world(t,cursor,op,4);return 1; // bhObjAspdSet (object light parameter)
    case 0xa4:world(t,cursor,op,4);return 1; // bhPlayerKaidanMotion
    case 0xbd:world(t,cursor,op,2);return 1; // bhReTryPointSet
    case 0xcc:world(t,cursor,op,4);return 1; // bhEneLookFlgSet
    // Exact operand lengths from event.c. Pure system-state changes are applied
    // here; effects needing world/audio/player services are reported as pending
    // world bindings with their exact operands.
    case 0x68:world(t,cursor,op,6);return 1; // bhLoadWork2
    case 0xb5:world(t,cursor,op,4);return 1; // bhLoadWorkEx (event.c:12808): wie 0x65, Ziel bhEtask[0]
    // event.c:7605 bhObjDposCk: v4 == 0 dreht obwp[v0].ay, sonst objP[v4].ang[0] des Gegners rom->enep[v0]
    // je Durchlauf um v3 Grad auf v2 Grad (Bit 2 von v1: Gegenrichtung). 1 = laeuft noch, 0 = fertig.
    case 0xb1:{v.require(p,6);const unsigned v0=v.u8(p+1),v1=v.u8(p+2),v3=v.u8(p+4),v4=v.u8(p+5);int v2=v.u8(p+3);advance(6);
        if(v4==0){
            if((v1&2)&&v2!=0)v2-=360;
            const int eay=int(v2*(65536.0f/360.0f)),eay2=int(v3*(65536.0f/360.0f));
            int ay=0;if(!object_ay_get||!object_ay_get(v0,ay))throw std::runtime_error("object yaw check requires native world binding");
            ay=int(std::int16_t(ay));
            if(eay!=int(std::uint16_t(ay))){
                if(eay<int(std::uint16_t(ay))){ay-=int(std::int16_t(eay2));if(eay>=int(std::uint16_t(ay))){object_ay_set(v0,int(std::uint16_t(eay)));return 0;}object_ay_set(v0,ay);return 1;}
                if(int(std::uint16_t(ay))<eay){ay+=int(std::int16_t(eay2));if(eay<=int(std::uint16_t(ay))){object_ay_set(v0,int(std::uint16_t(eay)));return 0;}object_ay_set(v0,ay);return 1;}
            }
            object_ay_set(v0,ay);return 0;}
        if((v1&2))v2=-v2;
        const int eay=int(std::int16_t(int(v2*(65536.0f/360.0f)))),eay2=int(std::int16_t(int(v3*(65536.0f/360.0f))));
        int ang=0;if(!enemy_node_ax_get||!enemy_node_ax_get(v0,v4,ang))throw std::runtime_error("enemy node angle check requires native world binding");
        ang=int(std::int16_t(ang));
        if(eay!=ang){
            if(eay<ang){ang-=eay2;if(eay>=ang){enemy_node_ax_set(v0,v4,eay);return 0;}enemy_node_ax_set(v0,v4,ang);return 1;}
            if(ang<eay){ang+=eay2;if(eay<=ang){enemy_node_ax_set(v0,v4,eay);return 0;}enemy_node_ax_set(v0,v4,ang);return 1;}
        }
        enemy_node_ax_set(v0,v4,ang);return 0;}
    // event.c:7794 bhEtcAtariEnePosSet: aus rom->posp[v2..v3) den Platz naechst owP[v5] des Gegners v0;
    // etcp[v1] wird darum zentriert, sys->evt_posno[v4] merkt ihn. Der Vergleich beginnt mit
    // posp[v2] gegen sich selbst (sw2 = Abstand von posp[v2]), wie im Original.
    case 0xb3:{v.require(p,8);const unsigned v0=v.u8(p+1),v1=v.u8(p+2),v2=v.u8(p+3),v3=v.u8(p+4),v4=v.u8(p+5),v5=v.u8(p+6);advance(8);
        if(v1>=interactions.size()||v4>=evt_posno.size()||v2>=positions_.size()||(v3>v2&&v3>positions_.size()))throw std::runtime_error("bhEtcAtariEnePosSet operand outside room tables");
        if(!enemy_joint_position)throw std::runtime_error("bhEtcAtariEnePosSet requires native enemy binding");
        const auto joint=enemy_joint_position(v0,v5);if(!joint)throw std::runtime_error("bhEtcAtariEnePosSet: owP[node] of the enemy work unavailable");
        const auto dist=[&](const NJS_POINT3& a){const float dx=a.x-joint->x,dy=a.y-joint->y,dz=a.z-joint->z;return std::sqrt(dx*dx+dy*dy+dz*dz);};
        float sw2=dist(positions_[v2]);unsigned cnt2=v2;
        for(unsigned cnt=v2;cnt<v3;++cnt){const float sw1=dist(positions_[cnt]);if(sw1<sw2){sw2=sw1;cnt2=cnt;}}
        auto& a=interactions[v1];const auto& pos=positions_[cnt2];
        a.position={pos.x-a.extent.x/2.0f,pos.y,pos.z-a.extent.z/2.0f};evt_posno[v4]=std::uint8_t(cnt2);return 1;}
    // event.c:7865 bhEtcAtariEvtPosSet: etcp[v1] um posp[evt_posno[v0]], itwp[v3] auf diesen Platz.
    case 0xb4:{v.require(p,4);const unsigned v0=v.u8(p+1),v1=v.u8(p+2),v3=v.u8(p+3);advance(4);
        if(v0>=evt_posno.size()||v1>=interactions.size()||evt_posno[v0]>=positions_.size())throw std::runtime_error("bhEtcAtariEvtPosSet operand outside room tables");
        auto& a=interactions[v1];const auto& pos=positions_[evt_posno[v0]];
        a.position={pos.x-a.extent.x/2.0f,pos.y,pos.z-a.extent.z/2.0f};
        if(!set_item_position)throw std::runtime_error("bhEtcAtariEvtPosSet requires native item binding");
        set_item_position(v3,pos);return 1;}
    case 0xbe:{ // bhPlyDposCk: turn the player yaw towards a target by a fixed step each pass
        if(!player_yaw||!set_player_yaw)throw std::runtime_error("player yaw check requires native player binding");
        int target=v.u8(p+3);const unsigned flags_byte=v.u8(p+2);const int step=v.u8(p+4);advance(6);
        if((flags_byte&2)&&target)target-=360;
        const int eay=int(target*(65536.0f/360.0f)),eay2=int(step*(65536.0f/360.0f));
        int ay=int(std::int16_t(player_yaw()&0xffff));const unsigned current=unsigned(ay)&0xffff;
        if(eay!=int(current)){
            if(eay<int(current)){ay=int(std::int16_t(ay-std::int16_t(eay2)));if(eay>=int(unsigned(ay)&0xffff)){set_player_yaw(unsigned(eay)&0xffff);return 0;}set_player_yaw(unsigned(ay)&0xffff);return 1;}
            ay=int(std::int16_t(ay+std::int16_t(eay2)));if(eay<=int(unsigned(ay)&0xffff)){set_player_yaw(unsigned(eay)&0xffff);return 0;}set_player_yaw(unsigned(ay)&0xffff);return 1;}
        return 0;}
    case 0x83:return advance(2); // bhSoundFadeOut: the original body only skips its operands
    case 0x89:change_player=v.u8(p+1);flags[10][0]|=0x80;advance(2);return 1; // bhPlayerChangeSet: cng_pid + cb_flg 0x80, verbraucht beim Raumladen (runtime.cpp load(), system.c:1717)
    // bhPlayerPoison2Cr (event.c:7369): loescht ausschliesslich den gespeicherten
    // Eintrag ply_stflg[0]. plp->stflg uebernimmt ihn erst beim Spielerwechsel
    // (player.c:663 bhPlayerChangeSet) - dieselbe Verzoegerung wie im Original.
    case 0xae:inventory_.ply_stflg[0]&=~0x200000u;return advance(2);
    case 0xa0:map_parameters={v.u8(p+1),v.u8(p+2),v.u8(p+4),v.u8(p+3)};flags[10][0]|=0x10000;advance(6);return 1; // bhMapSystemOn: mp_prm[0..3] (consumed by runtime)
    case 0xce:case 0xc5:case 0x87:world(t,cursor,op,2);return 1;
    // event.c:8906 bhExGameItemInit -> sub1.c:8553 ExtraGameItemInit: Battle-Game-Ausruestung in itm[cng_pid*16].
    case 0xcf:inventory_.extra_game_item_init(change_player,costume,(flags[16][0]&0x80000000u)!=0);if(change_player==inventory_.player)values[22]=inventory_.slots()[0];return advance(2);
    // event.c:3465 bhMutekiSetPl: plp->flg Bit 0x4. Solange es steht, ueberspringt der
    // Gegnerpass jeden Angriff auf den Spieler (zum Beispiel en17.c:1595).
    case 0x3a:{auto& flg=flags[13][0];if(v.u8(p+1))flg|=0x4u;else flg&=~0x4u;player_flags=flg;return advance(2);}
    // event.c:6849 bhTrapDamageSet: plp->stflg Bit 0x1000 — v0 == 0 SETZT es, jeder andere
    // Wert loescht es (im Original genau so herum).
    case 0xa1:if(!v.u8(p+1))flags[14][0]|=0x1000u;else flags[14][0]&=~0x1000u;return advance(2);
    case 0x76:world(t,cursor,op,4);return 1; // bh3dSoundFlagSet
    case 0xa5:world(t,cursor,op,4);return 1; // bhEneRenderSet: mdflg 0x200 (renderer binding pending)
    case 0x2c:world(t,cursor,op,4);return 1;  // bhEffectSet: bhSetEffect im geteilten eff[512]
    case 0x70:case 0x71:world(t,cursor,op,2);return 1; // bhEffPush/bhEffPop
    case 0x4e:world(t,cursor,op,14);return 1; // bhEffBloodSet: Blutstoss am Gegnerwork
    case 0x56:world(t,cursor,op,6);return 1;  // bhEffBloodPoolSet: Blutlache am Gegnerwork
    case 0x58:world(t,cursor,op,14);return 1; // bhEffBloodPoolSet2: Blutlache an einer Lage
    case 0x7f:world(t,cursor,op,6);return 1; // bhSoundPanSet
    // event.c:2574 bhSetNextRoomBgm: Byte 1 bleibt ungelesen, dann ComNextBgm, PointNextBgm,
    // OfsPointBgm (u16), NextBgmNo und FadeNextBgm = Byte * 10. Acht Byte.
    case 0x46:next_sound.bgm_com=v.u8(p+2);next_sound.bgm_point=v.u8(p+3);next_sound.bgm_offset=v.u16(p+4);
        next_sound.bgm_number=v.u8(p+6);next_sound.bgm_fade=unsigned(v.u8(p+7))*10u;return advance(8);
    // event.c:2612 bhSetNextRoomBgSe: Byte 1 waehlt den Raumtonplatz, danach ComNextBgSe,
    // PointNextBgSe, OfsPointBgSe (u16), NextBgSeNo (u16), FadeNextBgSe = Byte * 10 und
    // SetNextBgSeFlag (Byte 9 == 0 ergibt 1). Zehn Byte. Die Originalfelder haben zwei
    // Plaetze (sdfunc.c:39/40/105); ein groesserer Index schriebe dort hinter das Feld.
    case 0x47:{const unsigned slot=v.u8(p+1);
        // Ein groesserer Index schriebe im Original hinter das Feld; nativ wird der Befehl
        // dann nur gemeldet, statt fremden Speicher zu beschreiben oder etwas zu erfinden.
        if(slot>=next_sound.se_com.size()){world(t,cursor,op,10);return 1;}
        next_sound.se_com[slot]=v.u8(p+2);next_sound.se_point[slot]=v.u8(p+3);next_sound.se_offset[slot]=v.u16(p+4);
        next_sound.se_number[slot]=v.u16(p+6);next_sound.se_fade[slot]=unsigned(v.u8(p+8))*10u;
        next_sound.se_set[slot]=v.u8(p+9)==0?1u:0u;
        return advance(10);}
    // bhGameOverSet clears ts_flg bit 0x4000; the runtime starts gameover.c gov_md0.
    case 0xc6:flags[12][0]&=~0x4000u;world(t,cursor,op,2);return 1;
    // bhReturnTitleEvt (event.c:8859): two bytes; the runtime performs RequestAllStopSoundEx + bhReturnTitle.
    case 0xcd:world(t,cursor,op,2);return 1;
    // bhPadCheck: pad_ps holds newly pressed logical bits of this pass,
    // pad_on the held ones (bhSetPad). The 0xE000 mask (Select/Start/extra)
    // suppresses every check; ts_flg 0x80 hands the pad to the save screen.
    case 0x5d:{v.require(p,6);const unsigned bit=v.u8(p+1),mode=v.u8(p+2);advance(6);
        if(flags[12][0]&0x80u)return 1;
        if(!pad_state)throw std::runtime_error("pad condition requires native input binding");
        const auto state=pad_state();const unsigned ps=state.first,on=state.second;const unsigned mask=1u<<(bit&31u);const bool quiet=(ps&0xE000u)!=0;
        bool hit=false;switch(mode){
        case 0:hit=!quiet&&(ps&mask)!=0;break;
        case 1:hit=!quiet&&(on&mask)!=0;break;
        case 2:hit=!quiet&&((ps&mask)!=0||(on&mask)!=0);break;
        case 3:hit=!quiet&&ps!=0;break;
        case 4:hit=!quiet&&on!=0;break;
        case 5:hit=!quiet&&(ps!=0||on!=0);break;
        case 6:hit=!quiet&&(ps&0x200u)!=0&&(on&0x10u)==0;break;
        default:throw std::runtime_error("invalid bhPadCheck mode");}
        return hit?0:1;}
    case 0x69:{ // bhCommonCtr -> Common_controll (event.c:9963-12710): one pointer advance per command
        const unsigned command=v.u8(p+1);
        const auto length=common_controll_length(command,v.u8(p+2),v.u8(p+3));
        if(!length)throw std::runtime_error("unimplemented Common_controll command "+std::to_string(command));
        world(t,cursor,op,length);return 1;}
    default:
        // event.c:437 dm0 (Tabellenplaetze 0xD5-0xF2 und 0xF5-0xF7): return 0 ohne Vorschub. Der Scheduler
        // (event.c:13046) laesst die Aufgabe auf diesem Byte stehen; sie laeuft in jedem Durchlauf erneut
        // auf dm0 und kommt nie weiter. Einmal je Fundstelle auf der Konsole gemeldet (Diagnose).
        if((op>=0xd5&&op<=0xf2)||(op>=0xf5&&op<=0xf7)){
            if(reported_.insert({op,p}).second)std::fprintf(stderr,"EVENT dm0: Ereignis %u, Ebene %u, Offset 0x%zx, Opcode 0x%02x: unbelegter Tabellenplatz (event.c:437), die Aufgabe steht hier dauerhaft\n",t.event,t.level,p-base_,op);
            return 0;}
        {std::ostringstream s;s<<"unimplemented opcode 0x"<<std::hex<<op;throw std::runtime_error(s.str());}
    }
}
void EventVm::run(Task& t,bool scenario,std::size_t entry){
    if(!scenario&&t.blocked)return;
    // bhScePtr is separate from bhCetask->data. A self Event_exec changes data,
    // but the running pass continues at its current instruction stream.
    std::size_t cursor=scenario?entry:t.pc;
    try{
        for(unsigned count=0;count<10000;++count){
            if(instruction(t,cursor)!=0)continue;
            if(ifel_>0){
                gsp_-=4;--ifel_;if(gsp_<0)throw std::runtime_error("event G_Sp outside script");
                cursor=std::size_t(gsp_);continue;
            }
            if(!scenario)t.pc=cursor;
            return;
        }
        throw std::runtime_error("event instruction budget exceeded");
    }catch(const std::exception& e){
        // Ein Originalpass endet immer mit bhIfelFlg <= 0 (event.c:13082); bricht der Port einen Pass an einer
        // Luecke ab, darf das offene IF/G_Sp nicht in den naechsten Taskpass durchschlagen (sonst springt dessen
        // Schlaf nach bhEnd auf die Ruecksprungadresse dieses Passes).
        ifel_=0;
        if(!scenario){t.blocked=true;t.pc=cursor;}
        std::ostringstream s;s<<"event "<<t.event<<", task "<<t.level<<", offset 0x"<<std::hex<<(cursor-base_);
        if(cursor<room_.size())s<<" (opcode 0x"<<unsigned(room_[cursor])<<")";
        else s<<" (outside room)";
        s<<": "<<e.what();
        if(std::find(errors.begin(),errors.end(),s.str())==errors.end())errors.push_back(s.str());
    }
}
void EventVm::advance_game_clock(unsigned vertical_blanks){
    if(flag(1,3))inventory_.play_time+=std::min(vertical_blanks,216000000u-std::min(inventory_.play_time,216000000u));
    // bhControlVSync: the host's 30-Hz game pass accounts for its two VSyncs
    // through vertical_blanks, just like stv_tm above.  The original timers
    // are configured in even VSync units, so their subtraction cannot cross
    // zero in ordinary gameplay.
    if((flags[6][0]&0x200u)&&event_deadline_ticks){
        event_deadline_ticks-=vertical_blanks;
        const unsigned keep=event_deadline_mode==1?69:event_deadline_mode==2?67:event_deadline_mode==3?70:~0u;
        if(keep!=~0u&&!flag(1,keep)){event_deadline_mode=0;event_deadline_ticks=0;}
    }
    // bhMainSequence calls bhCheckEvtTimer after the VSync service.
    check_event_deadline();
}
bool EventVm::event_deadline_visible()const{return event_deadline_ticks&&(flags[6][0]&0x200u)&&!flag(3,349);}
void EventVm::check_event_deadline(){
    // system.c draws mode 3's deadline and dispatches its warning voices from
    // the same game pass.  The countdown itself was already advanced by
    // bhControlVSync above.
    if(event_deadline_ticks&&event_deadline_mode==3&&(flags[6][0]&0x200u)&&event_deadline_voice_tick>=event_deadline_ticks){
        event_deadline_voice_tick=event_deadline_voice_count?event_deadline_voice_tick-60:600;
        if(!(flags[12][0]&0x80u)&&!(flags[10][0]&1u)&&system_voice)system_voice(event_deadline_voice_count);
        ++event_deadline_voice_count;
    }
    // bhCheckEvtTimer (player.c): it deliberately leaves a zero evt_tim and
    // the mode installed until the normal failure/game-over lifecycle clears
    // them.  All gating flags are retained, rather than treating a host tick
    // as an unconditional timeout.
    if(event_deadline_mode==0||event_deadline_ticks!=0||(flags[6][0]&0x200u)==0||(flags[11][0]&0x20000000u)==0||(flags[12][0]&0x80u)||(flags[12][0]&0x15e00u)!=0x15e00u||(flags[10][0]&1u))return; /* player.c:1272: ts_flg 0x4000 (Task 14 frei), 0x1000, 0x200, 0x400, 0x800, 0x10000 */
    if(flag(1,69)){
        if(flags[5][0]&0x200u){flags[5][0]|=0x400000u;flags[10][0]|=0x2000u;flags[10][0]&=~0x1000u;}
        // player.c:1284 bhSetEffect(26, &pnt, NULL, 0): pnt ist uninitialisiert;
        // bhEff026 ueberschreibt px/py in mode0 0 und liest pz/lox.. nicht.
        if(set_effect)set_effect(26);
        inventory_.hp=-1;set_flag(1,69,1);
    }
    if(flag(1,67)){
        flags[6][0]=0x20;flags[5][0]&=~0x200u;flags[11][0]|=0x400u;flags[12][0]&=~0x4000u;flags[12][0]|=0x100u;player_flags|=0x10002u;game_over_requested=true;set_flag(1,67,1);
    }
    if(flag(1,70)){
        if(system_voice)system_voice(11);
        flags[6][0]=0x20;flags[5][0]&=~0x200u;flags[11][0]|=0x400u;flags[12][0]&=~0x4000u;flags[12][0]|=0x100u;player_flags|=0x10002u;game_over_requested=true;set_flag(1,70,1);
    }
}
void EventVm::control_exm(){
    // bhMainSequence (game.c:115-286) for the bhCheckExmAtari requests of this pass.
    auto& st=flags[5][0];auto& sp=flags[6][0];auto& cb=flags[10][0];auto& gm=flags[11][0];
    const unsigned pad_ps=pad_cleared_||!pad_state?0u:pad_state().first; // hitchk.c: sys->pad_ps = 0
    if((st&0x200000)&&!(cb&1)){ // :115
        st&=~0x200080u;if(cb&0x20000000){cb|=0x1000;cb&=~0x20000000u;}if(!(cb&4))st&=~4u;
        sp=0xffffffffu;if(presentation_flags)presentation_flags(0,3);
        if(!(gm&0x20000)){st&=~1u;gm|=0x800;}}
    message_release(); // :147
    if(cb&0x8){ // :196
        cb&=~0x8u;sp=0x38;if(!(exm_attr&0x20))sp|=0x80;
        if(exm_attr&0x2){if(presentation_flags)presentation_flags(1,0);if(exm_light_off)exm_light_off();}
        if(exm_attr&0x4){if(presentation_flags)presentation_flags(2,0);}
        if(exm_attr&0x10)sp|=0x2;}
    if(st&0x80){ // :227
        if((gm&0x10000)&&(gm&0x20000))sp|=0x40;
        if((pad_ps&0x200)&&!(st&0x200))st|=0x200000;}
    if(cb&0x20){ // :239
        cb&=~0x20u;sp=0x30;if(st&0x80){sp|=0x8;if(!(exm_attr&0x20))sp|=0x80;}
        if(exm_attr&0x10)sp|=0x2;}
    message_complete(); // :264
    if(gm&0x1000){ // :274 bhSetFixedCut / bhSetEventFixedCut clear gm_flg 0x1000 and set st_flg 0x1
        gm&=~0x1000u;st|=1;
        if(!(gm&0x10000)){if(fixed_cut)fixed_cut(fixcno);}else if(event_fixed_cut)event_fixed_cut(fixcno,fixkno);}
    // :286 bhCheckCut(1) (cut.c:12-64): erzwungene Rueckkehr auf den Spielerschnitt, unter gm_flg 0x40
    // der Augenzweig (RoomWorld::return_to_cut loescht gm_flg 0x800 dort selbst).
    if((gm&0x800)&&(sp&0x40)&&!(st&1)){if(!(gm&0x40))gm&=~0x800u;if(return_cut)return_cut();}
    pad_cleared_=false;main_sequence_done_=true;
}
void EventVm::message_release(){
    // game.c:147: das Meldungsende ist genau einen bhControlEvent-Pass sichtbar; danach gibt
    // bhMainSequence die Meldung frei (st_flg 0x200, CineSet 0x2000, Untersuchungskamera 0x80).
    auto& st=flags[5][0];auto& sp=flags[6][0];auto& cb=flags[10][0];auto& gm=flags[11][0];
    if(!(st&0x400000)||(cb&1))return;
    if(cb&0x20000000){cb|=0x1000;cb&=~0x20000000u;}
    if(cb&0x200000)st&=~0x200u;
    else{
        st&=~0x400200u;
        if(st&0x2000){
            sp=0xffffffffu;st&=~0x2200u;if(!(cb&4))st&=~4u;
            pad_cleared_=true;} /* :175-177 pad_on/ps/old := pad_*b, der Host liest das Pad je Durchlauf nur einmal */
        if(st&0x80){st&=~0x80u;if(presentation_flags)presentation_flags(0,3);if(!(gm&0x20000)){st&=~1u;gm|=0x800;}}
        end_message();}
}
void EventVm::message_complete(){
    // game.c:264: die fertige letzte Seite schliesst OK oder Cancel (pad_ps 0x800/0x1000); ohne
    // Taste schliesst die beantwortete Frage, sobald der Text ihr Ende erreicht (cb_flg 0x1000).
    auto& st=flags[5][0];auto& cb=flags[10][0];
    const unsigned pad_ps=pad_cleared_||!pad_state?0u:pad_state().first;
    if((st&0x200)&&(st&0x4000)&&(st&0x1000)&&((pad_ps&0x800)||(pad_ps&0x1000)||message_end_pending_||((cb&0x1000)&&!(cb&1)))){
        st|=0x400000u;cb|=0x2000u;cb&=~0x1000u;message_end_pending_=false;}
}
void EventVm::tick(){
    // bhMainSequence laeuft auch dann, wenn der Aufrufer nur den Ereignispass faehrt (Testharness,
    // Terminalzweig); im Runtimepfad hat control_exm die beiden Schritte schon erledigt.
    if(!main_sequence_done_){message_release();message_complete();pad_cleared_=false;}
    main_sequence_done_=false;
    ++tick_count;service_pickup();const bool cinematic=flags[10][0]&4;if(dialog_.active())values[7]=dialog_.selection;/* bhSysCallEvent: bhControlMessage vor bhControlEvent, mit den Tasten dieses Durchlaufs (message.c:50). */{const auto step=dialog_.update();if(dialog_.moved()&&system_se)system_se(2); /* message.c:275/284 */if(step==MessageDialog::Step::Answered)close_dialog(true);else if(step==MessageDialog::Step::Closed){flags[10][0]|=0x2000;if(flags[5][0]&0x2000){flags[5][0]&=~0x2204u;flags[6][0]=0xffffffff;} /* message.c:312 */if(opening_hint_)status_requested=true;opening_hint_=false;finish_pickup();}if(dialog_.active())values[7]=dialog_.selection;
    // sys->st_flg ist eine Variable: die Meldungsbits, die bhControlMessage schreibt (0x1000
    // Seite fertig, 0x4000 Ende, 0x8000 Auswahl, 0x800/0x400 Wartezeit, 0x80000 Vorauswahl),
    // liest das Raumskript per bhFlagCk Typ 5 (RM_1050 Ereignis 34: WHILE !(st_flg & 0x1000),
    // dann WHILE (st_flg & 0x1000) bis zum Umblaettern, erst danach bhCamSet 4).
    flags[5][0]=(flags[5][0]&~0x8dc00u)|(dialog_.status()&0x8dc00u);}(menu_open||status_use_pending_)?flags[5][0]|=8:flags[5][0]&=~8u;if(dialog_.active()||message_end_pending_||(flags[5][0]&0x400000u))flags[5][0]|=0x200;else flags[5][0]&=~0x200u;ifel_=0; /* bhScenarioCheck: bhIfelFlg = 0 */run(tasks_[current_task_],true,scd1_);
    // bhEventScheduler2 (event.c:13024) laeuft nur mit sp_flg 0x10; die Raeume setzen bzw. loeschen
    // das Bit selbst im scd1 (bhSet 6,27), z. B. RM_0000 in jedem Durchlauf.
    schedule();
    // ItemUse/Use_01/05 call StatusCancel, whose animated exit keeps st_flg
    // 8 for bhControlEvent. The host closes visually at once; retain that
    // handover through one complete scenario/scheduler pass.
    status_use_pending_=false;if(!menu_open)flags[5][0]&=~8u;door_scenario_pending_=false;
    // game.c:270 loescht cb_flg 0x1000 nicht mehr in diesem Durchlauf: bhControlMessage setzt die
    // Antwort (message.c:244), der naechste Durchlauf wertet sie in game.c:264 aus und loescht sie dort.
    // bhCheckSubTask after bhControlEvent (system.c:736-744): the same pass in
    // which cb_flg 4 falls releases the player work the cinematic held and drops
    // the skip request. This is the only original site that clears plp->flg
    // 0x10000 (bhControlPlayerPad gate, player.c:1804) and plp->stflg 0x18000,
    // and it resets plp->mode3 so bhCPM0_action returns to PlMtnAct (player.c:2098).
    if(cinematic&&!(flags[10][0]&4)){
        flags[13][0]&=~0x10000u;flags[14][0]&=~0x18000u;flags[10][0]&=~0x10000000u;
        player_flags=flags[13][0];
        if(release_cinematic_player)release_cinematic_player();
    }}
Message EventVm::room_message(unsigned index)const{return read_message(room_,messages_,index);}
bool EventVm::request_door(const DoorRequest& request,bool script){
    if(!door||!door(request))return false;
    // bhSetDoorDemo / bhCheckExmAtari signal the request now; game.c starts
    // its fade on the following main pass, after bhControlEvent has run.
    if(script){
        // room.c:887-908 bhSetDoorDemo (DoorCall): sp_flg = 0x48 (|0x200) sofort, plp->stflg |= 0x80010000,
        // cb_flg |= 1, st_flg &= ~0x400200 (st_flg 0x4 setzt nur der Untersuchungspfad).
        flags[6][0]=(flags[6][0]&0x200u)|0x48u;
        flags[14][0]|=0x80010000u;flags[10][0]|=1u;flags[5][0]&=~0x400200u;
    }else{
        // hitchk.c:4660-4663 bhCheckExmAtari: pp->stflg |= 0x80000000, cb_flg |= 1, st_flg |= 4.
        flags[14][0]|=0x80000000u;flags[10][0]|=1u;flags[5][0]|=4u;
    }
    door_scenario_pending_=true;return true;
}
void EventVm::finish_room_exit(){if(door_scenario_pending_)tick();}
void EventVm::disable_item(const Task& task,unsigned op,std::size_t pc,unsigned item){
    // bhItmCk/bhItmSetCk: sys->itwp[item].stflg |= 0x1000000 (event.c:2127/2786). Das sperrt
    // bhControlObjItm und bhDrawObjItm (objitm.c:407/:559); mdflg (bhSetDispObj) bleibt unberuehrt.
    if(item>=room_items_.size())throw std::runtime_error("pickup model outside room");
    if(item_disable){item_disable(item);return;}
    if(reported_.insert({op,pc}).second)pending_bindings.push_back(EventCommand{op,task.level,task.event,pc-base_,{room_.begin()+pc,room_.begin()+pc+6}});
}
std::vector<EventTaskState> EventVm::task_states()const{std::vector<EventTaskState> out;for(const auto& t:tasks_)if(t.active)out.push_back({t.event,t.level,t.delay,t.pc-base_,t.blocked,t.wpnl_no});return out;}
void EventVm::choose_dialog(int direction){dialog_.queue_move(direction);} // pad_ps 0x4/0x8 fuer den naechsten bhControlMessage-Durchlauf
void EventVm::finish_pickup(){if(pickup_display_)flags[10][0]&=~0x10u;pickup_item_=pickup_display_=pickup_mode_=0;pickup_decision_pending_=false;}
void EventVm::show_dialog(Message message,unsigned index){
    // message.c bhSetMessage: neither a previous question's end/response nor
    // its page marker may acknowledge the newly started message.
    flags[5][0]=(flags[5][0]&~0x8dc00u)|0x200;
    flags[10][0]&=~0x20003000u;message_end_pending_=false;
    dialog_.show(std::move(message),index);values[7]=dialog_.selection;
}
void EventVm::pickup_result(const PickupResult& result){
    pickup_mode_=result.mode;
    // GetItem versteckt kein Raummodell; das tut allein bhItmCk des Raumskripts auf cb_flg 0x800.
    if(result.granted){values[22]=inventory_.slots()[0];values[18]=inventory_.weapon();}
    if(result.message)show_dialog(item_message(result.message,pickup_display_),result.message);
    else if(result.used||result.mode==3||result.mode==5){dialog_=MessageDialog{};finish_pickup();flags[5][0]&=~0x200u;flags[10][0]|=0x2000;}
}
void EventVm::begin_pickup(unsigned item){
    if(item==130)item=35;if(!item||item>=156)throw std::runtime_error("script pickup item outside original table");
    pickup_display_=pickup_item_=item;values[8]=item;
    // StatusMain clears the acknowledgement before starting each GetItem.
    begin_status();flags[10][0]=(flags[10][0]&~0x800u)|0x10;
    // sub1.c:3452 StatusMain case 8: GetItem laeuft erst nach SpriteH, wenn statusflg
    // 0x40 (Einfahrt) gefallen und 0x10 (ItemView zeigt das Modell, itemview.c:380)
    // gesetzt ist. testmode 0 entscheidet also erst bei stehendem Bildschirm; die Frage
    // 157/154/153 (mes_spos[1], feste Position) liegt nie ueber den einfahrenden Sprites.
    pickup_mode_=0;pickup_decision_pending_=true;service_pickup();
}
void EventVm::begin_file(unsigned item){
    // StatusMain/GetFile: FILE requests acknowledge the room pickup, but only
    // set itm[383]. They never pass through GetItem's inventory-slot insertion.
    values[8]=item;begin_status();acquired_file_=original_file_number(item,values[0],values[1]);
    inventory_.items[383]|=1u<<acquired_file_;flags[10][0]|=0x20800; // das Modell versteckt bhItmCk (cb_flg 0x800)
}
void EventVm::service_item_request(){
    // bhCheckSubTask accepts scripted acquisition even while CineSet is active.
    // It waits for existing modal/system tasks, never for the room identity.
    if(!(flags[10][0]&0x24010)||pickup_display_||acquired_file_!=~0u||menu_open||dialog_.active()||(flags[5][0]&0x1c040008u)||(flags[10][0]&3)||!item_message)return;
    // bhCheckSubTask opens the status task for cb_flg 0x64010; StatusMain then picks the screen:
    // 0x10 = GetItem (sb_id), anything else without box/file bits = the normal status screen.
    // GetItem consumes 0x4000 itself (sub1.c ~3857); a plain script request is consumed here.
    if(flags[10][0]&0x20000){begin_file(values[8]);return;}
    if(flags[10][0]&0x10){begin_pickup(values[8]);return;} // cb_flg 0x4000 verbraucht GetItem testmode 0 selbst (sub1.c:3830-3857), das erst im stehenden Bildschirm entscheidet
    flags[10][0]&=~0x4000u;status_requested=true;
}
void EventVm::cancel_item_request(){
    // Kein Originalzweig: der Hostschutz für Bestandsänderungen aus dem
    // Debugmenü. GetItem selbst entscheidet unverändert nur einmal.
    if(!pickup_display_&&acquired_file_==~0u)return;
    dialog_=MessageDialog{};flags[5][0]&=~0x200u;flags[10][0]&=~0x1000u;
    finish_pickup();finish_file();
}
void EventVm::cancel_dialog(){
    // game.c:264: Cancel (pad_ps 0x1000) schliesst nur die fertige letzte Seite; der Druck selbst
    // wird in bhMainSequence vor bhControlMessage ausgewertet (control_exm :264). Eine Auswahl
    // (auch GetItem, sub1.c:3877) und Zwischenseiten kennen kein Cancel; das liest dort allein
    // das Raumskript (bhPadCheck, RM_1090 Event 13).
    if(dialog_.active()&&dialog_.closable())finish_page();
}
void EventVm::confirm_dialog(){
    // game.c:264 schliesst die fertige letzte Seite noch vor bhControlMessage (control_exm :264);
    // eine offene Auswahl beantwortet dagegen bhControlMessage selbst (message.c:231-264), die
    // Meldung bleibt bis game.c:147 stehen. Alles andere blaettert tick um.
    if(!dialog_.active())return;
    if(dialog_.choosing()){dialog_.queue_ok();return;}
    if(dialog_.closable())finish_page();else dialog_.queue_ok();
}
void EventVm::finish_page(){
    // Der Host nimmt die fertige Seite sofort vom Bild; die Skriptfreigabe bleibt wie im Original
    // einen Durchlauf sichtbar (game.c:264 setzt st_flg 0x400000, game.c:147 loescht es).
    dialog_.close();if(audio_stop)audio_stop(1);message_end_pending_=true;finish_pickup();
}
void EventVm::close_dialog(bool answered){
    // bhControlMessage, Auswahl mit OK bestaetigt (message.c:231-262): mes_sel meldet die
    // Wahl. Die Meldung bleibt aktiv; game.c:264 verlangt danach st_flg 0x1000 und game.c:147
    // gibt sie im naechsten Durchlauf frei (Meldungsende, CineSet, Untersuchungskamera).
    values[7]=unsigned(dialog_.selection)+(answered&&menu_open?100u:0u);
    if(pickup_item_&&answered){
        // GetItem testmode 1/4 (sub1.c:3877/3926): CallSystemSe(0,3), dann loeschen die
        // Antwort cb_flg 0x1000 und st_flg 0x200; die Frage verschwindet also, bevor der
        // naechste Durchlauf 158 meldet.
        if(system_se)system_se(3);
        const unsigned answer=100+unsigned(dialog_.selection);
        dialog_=MessageDialog{};flags[5][0]&=~0x200u;
        pickup_result(inventory_.pickup(pickup_item_,flags[10][0],values[0],values[1],pickup_mode_,int(answer),status_busy&&status_busy()));return;
    }
    // message.c:242-262: die beantwortete Frage meldet cb_flg 0x1000 und st_flg 0x4000, der
    // Abschluss der skripteigenen Meldung (st_flg 0x2000) dagegen cb_flg 0x20002000 und
    // st_flg 0x400000. :264/:147 werten das im naechsten Durchlauf aus.
    auto& st=flags[5][0];
    if(!(st&0x2000)){flags[10][0]|=0x1000;st|=0x4000;}
    else{
        if((flags[11][0]&0x10000)&&(flags[11][0]&0x20000))flags[6][0]|=0x40;
        flags[10][0]|=0x20002000u;
        if(st&0x80)st|=0x200000u;
        st|=0x400000;}
    dialog_.close();if(audio_stop)audio_stop(1);message_end_pending_=true;finish_pickup();
}
void EventVm::end_message(){
    // game.c:162/168: st_flg 0x200 erloschen. Der Host gibt die Anzeige im selben Zug frei
    // (Meldungsstimme, Aufnahmehandover, Untersuchungshinweis).
    message_end_pending_=false;
    dialog_.close();if(audio_stop)audio_stop(1);
    finish_pickup();if(opening_hint_)status_requested=true;opening_hint_=false;
}
void EventVm::service_pickup(){
    // StatusMain ruft GetItem in jedem Durchlauf. Nach der Einfügung (testmode
    // 2) folgt dort die Meldung 158 und testmode 3; beides passiert erst im
    // nächsten Durchlauf und wartet auf statusflg 0x40 / st_flg 0x200.
    if(!pickup_item_)return;
    const bool busy=status_busy&&status_busy();
    if(pickup_decision_pending_){ // testmode 0, erst ohne statusflg 0x40 (sub1.c:3452)
        if(busy)return;
        pickup_decision_pending_=false;pickup_result(inventory_.pickup(pickup_item_,flags[10][0],values[0],values[1]));return;}
    if(pickup_mode_!=2||dialog_.active())return;
    pickup_result(inventory_.pickup(pickup_item_,flags[10][0],values[0],values[1],2,-1,busy));
}
void EventVm::begin_status(){
    flags[10][0]&=~0x400u; // sub1.c StatusMain entry
    // sub1.c:3392-3399 StatusMain case 0x1 (jede Oeffnung: Status, GetItem, Datei, Kiste):
    // gm_mode 3 setzt gm_flg 0x8000000 fest, sonst bhCheckSubPack. Die Tasche (RM_4050
    // Ereignis 0) setzt nur ev_flg 6; erst diese Ableitung gibt die zwei Plaetze frei.
    if(inventory_.game_mode==3){flags[11][0]|=0x8000000u;inventory_.side_pack=true;}
    else check_sub_pack();
}
void EventVm::check_sub_pack(){
    if(inventory_.player>2)return; // player.c:1076: ply_id 3 laesst gm_flg stehen
    if(flag(1,6+inventory_.player))flags[11][0]|=0x8000000u;else flags[11][0]&=~0x8000000u;
    inventory_.side_pack=bool(flags[11][0]&0x8000000u);
}
bool EventVm::use(unsigned item,unsigned* message,unsigned slot){
    if(message)*message=0;if(!item||item>=156||!inventory_.contains(item))return false;
    if(slot==~0u){slot=1;while(slot<12&&Inventory::id(inventory_.slots()[slot])!=item)++slot;}
    if(!slot||slot>=12||Inventory::id(inventory_.slots()[slot])!=item)return false;
    const Interaction* area=nullptr;
    if((flags[10][0]&0x200)&&values[16]<floors_.size())area=&floors_[values[16]];
    // ItemCommand -> original ItemUse for every item type, including its messages.
    const auto result=inventory_.use_item(slot,area?&area->parameters:nullptr,flags[10][0],values[0],values[1],0);
    if(message)*message=result.message;
    if(!result.closing)return false;
    values[8]=item;status_use_pending_=true;return true;
}
void EventVm::check_floor(NJS_POINT3 pos,Angle yaw,int floor){
    if(floor_binding){floor_binding(pos,yaw,floor);return;}
    // bhControlPlayer does not run bhCheckFloorP while sp_flg bit 0 is
    // disabled (bhMessageSet/Cinematic scripts). Preserve the last floor
    // work instead of retriggering a paused player's area every host tick.
    if(!(flags[6][0]&1))return;
    flags[10][0]&=~0x200u;if(flags[13][0]&6)return;
    const auto direction=(yaw+8192)&0xc000;
    for(unsigned i=0;i<floors_.size();++i){const auto& a=floors_[i];if(!(a.flags&1)||a.type!=0)continue;
        auto probe=pos;
        if(a.attribute&1){if(a.floor!=floor)continue;probe.x-=3*njSin(yaw);probe.z-=3*njCos(yaw);}
        else if(pos.y<a.position.y||pos.y>a.position.y+a.extent.y)continue;
        if(probe.x<a.position.x||probe.x>a.position.x+a.extent.x||probe.z<a.position.z||probe.z>a.position.z+a.extent.z)continue;
        if((direction==32768&&(a.attribute&0x400))||(direction==16384&&(a.attribute&0x800))||(direction==0&&(a.attribute&0x1000))||(direction==49152&&(a.attribute&0x2000)))continue;
        flags[10][0]|=0x200;values[16]=i;
    }
}
// bhSysCallGame: any running cinematic (cb_flg 4, CineSet 0/3/5) may be skipped once, never
// with menu/movie/system states st_flg 0x1C040008 active. Cleared when it ends (see tick).
bool EventVm::skip_cinematic(){if(!(flags[10][0]&4)||(flags[5][0]&0x1C040008u)||(flags[10][0]&0x10000000))return false;flags[10][0]|=0x10000000;return true;}
bool EventVm::interact(NJS_POINT3 pos,Angle yaw,int floor){return interact(pos,yaw,floor,pos,0.0f);}
bool EventVm::interact(NJS_POINT3 pos,Angle yaw,int floor,NJS_POINT3 front,float arn){
    // Das Tor steht in exm_gate (player.c:1678); st_flg 0x4 ist dort keine Bedingung (jede Sonderaktion
    // setzt ein maskiertes Bit: cro/Kiste stflg 0x10000, rpsh/psh stflg 0x10080, kdu/hsu/psh flg 0x4).
    // Dialog und Statusbildschirm vertreten die Originaltasks (bhMessageSet v2 0: sp_flg &= ~7; Status: sp_flg 0x30).
    if(dialog_.active()||menu_open)return false;
    const auto direction=(yaw+8192)&0xc000;
    /* hitchk.c:4601-4618: pxn[0] und pxn[3] gehen von EXP_WORK fpx/fpz aus (vor den
       Wandpaessen, player.c:1585), pxn[1]/[2]/[4] von plp->px/pz nach ihnen. Gegen eine
       Wand gelaufen liegt fpx um den Schub weiter vorn; die Tuersonde reicht damit im
       Gehen so weit wie im Original. Der Eintrag waehlt seine Sonde mit exp->type. */
    float sine,cosine;njSinCos(yaw,&sine,&cosine);
    const float pxn[5]={front.x-sine*(1.5f+(3.0f-arn)),pos.x-0.75f*sine,pos.x-4.5f*sine,front.x-sine*(3.0f+(3.0f-arn)),pos.x-2.0f*sine};
    const float pzn[5]={front.z-cosine*(1.5f+(3.0f-arn)),pos.z-0.75f*cosine,pos.z-4.5f*cosine,front.z-cosine*(3.0f+(3.0f-arn)),pos.z-2.0f*cosine};
    for(unsigned i=0;i<interactions.size();++i){const auto& a=interactions[i];
        /* Typen ueber 4 kommen in keiner Raumtabelle vor; pxn[type] laege dort ausserhalb
           der Tabelle (hitchk.c:4639). Sie behalten die bisherige Host-Sonde (Luecke). */
        const auto probe=a.type<5?NJS_POINT3{pxn[a.type],pos.y,pzn[a.type]}:NJS_POINT3{pos.x-2.0f*sine,pos.y,pos.z-2.0f*cosine};if(!(a.flags&1)||a.floor!=floor||a.type==1||a.type==2||probe.x<a.position.x||probe.x>a.position.x+a.extent.x||probe.z<a.position.z||probe.z>a.position.z+a.extent.z)continue;
        // hitchk.c:4642-4644: cb_flg 0x100 und etc_idx fuer jeden Treffer vor dem switch; eine
        // abgelehnte Blickrichtung loescht nur 0x100 wieder, etc_idx bleibt stehen.
        values[20]=i;flags[10][0]|=0x100;
        const bool facing_blocked=(direction==32768&&(a.attribute&0x400))||(direction==16384&&(a.attribute&0x800))||(direction==0&&(a.attribute&0x1000))||(direction==49152&&(a.attribute&0x2000));
        if(a.type==0){if(facing_blocked){flags[10][0]&=~0x100u;continue;}
            if(request_door(DoorRequest{a.attribute,a.parameters[0],a.parameters[1],a.parameters[2],a.parameters[3],values[21]},false))return true;
            flags[10][0]&=~0x100u;return false;}
        if((a.type==3||a.type==4)&&facing_blocked){flags[10][0]&=~0x100u;continue;}
        if(a.type==4){ // hitchk.c:4847-4925
            if(!(a.attribute&2)){
                // Ohne Weltbesitzer (VM-Fixtures) gilt der geladene itwp-Stand aus rom->itmp (room.c:141).
                unsigned item_flg=0,item_id=0;
                // Raumitems (< rom->itm_n, bhSetItem room.c:141) leben im Weltbesitzer; hoehere Plaetze nur im Pool.
                // Ein Skriptzugriff auf itwp[n >= itm_n] (Position, aspd, stflg) legt dort zwar ein Weltwork an,
                // aendert im Original aber flg/id des Pools nicht.
                const bool room_item=a.parameters[0]<room_items_.size();
                const bool known=room_item&&(item_work?item_work(a.parameters[0],item_flg,item_id):(item_flg=room_item_flags_[a.parameters[0]],item_id=room_items_[a.parameters[0]],true));
                // sys->itwp ist ein fester Pool aus 32 Plaetzen; ab rom->itm_n gilt der Inhalt eines frueheren
                // Raums oder die Nullung von bhInitObjItm (objitm.c:147). Ohne flg 0x1 faellt hitchk.c:4865
                // in den Objektzweig (:4903, obwp[prm0]), z. B. die Kiste in RM_0160 ATR 4.
                if(!known&&a.parameters[0]<item_pool_flg_.size()){item_flg=item_pool_flg_[a.parameters[0]];item_id=item_pool_id_[a.parameters[0]];}
                else if(!known){
                    const auto gap="bhCheckExmAtari: ATR "+std::to_string(i)+" liest itwp["+std::to_string(a.parameters[0])+"] ausserhalb rom->itm_n";
                    if(std::find(errors.begin(),errors.end(),gap)==errors.end())errors.push_back(gap);
                    flags[10][0]&=~0x100u;return false;}
                if(a.attribute&0x10)flags[10][0]|=0x20000; // GetFile
                if(item_flg&1){
                    values[8]=item_id; // sys->sb_id = op->id
                    if(a.attribute&1){ // bhCPM2_act_cro (mode2 19) meldet cb_flg 0x10 erst am Motionende
                        flags[13][0]|=0x10000;player_flags=flags[13][0];flags[14][0]|=0x10000;flags[5][0]|=4;
                        if(!set_player_mode)throw std::runtime_error("crouch pickup requires native player binding");
                        set_player_mode(1u|(19u<<16));
                    }else{flags[5][0]&=~4u;flags[10][0]|=0x10;}
                    return true;}
                // Ohne itwp flg 1 laeuft das Original in den Kistenzweig weiter (prm0 dann als obwp-Index).
            }else if(a.attribute&0xc)flags[10][0]|=(a.attribute&4)?0x80000u:0x100000u;
            if(a.parameters[0]==0xff)flags[10][0]|=0x40000;
            else{if(!item_box_object)throw std::runtime_error("item box object requires native world binding");
                // op->type = 100; *(int*)&op->mode0 = 0; pp->flg |= 0x10000; pp->stflg |= 0x18000; sys->st_flg |= 0x4; *(int*)&pp->mode0 = 1
                if(item_box_object(a.parameters[0])){flags[13][0]|=0x10000;player_flags=flags[13][0];flags[14][0]|=0x18000;flags[5][0]|=4;
                    if(!set_player_mode)throw std::runtime_error("item box object requires native player binding");
                    set_player_mode(1u);}}
            return true;}
        // hitchk.c:4786-4843 (case 3): both the camera and the message wait for st_flg 0x80 to clear.
        if(a.type==3&&!(flags[5][0]&0x80)){
            if(a.attribute&9){
                fixcno=a.parameters[0];fixkno=a.parameters[2];exm_attr=a.attribute;pad_cleared_=true;
                flags[11][0]|=0x1000;if(a.attribute&8)flags[11][0]|=0x10000;else flags[11][0]&=~0x10000u;
                flags[5][0]|=0x80|0x4;flags[10][0]|=0x8;
                if(exm_cut_effect)exm_cut_effect(); // cam.flg 0x20 -> ef_flg 0x10
            }
            if(a.attribute&0x8000){
                // hitchk.c bhCheckExmAtari -> bhSetMessage: interaction work,
                // response/end bits and page selection start afresh.
                show_dialog(read_message(room_,messages_,a.parameters[1]),a.parameters[1]);
                if(!(a.attribute&9))pad_cleared_=true;
                flags[10][0]|=0x20;flags[5][0]|=0x2204;
            }
        }
        return true;
    }return false;
}
}
