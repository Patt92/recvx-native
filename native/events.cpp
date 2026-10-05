#include "events.h"
#include "binary_view.h"
#include "room.h"
#include "ninja_math.h"
#include <algorithm>
#include <sstream>
namespace recvx {
EventVm::EventVm(const std::vector<std::uint8_t>& room,Inventory& inv,MessageDialog& dialog):room_(room),inventory_(inv),dialog_(dialog){
    BinaryView v(room_);const auto info=inspect_room(room_);base_=info.event_offset;if(!base_)throw std::runtime_error("room event section missing");
    // EVT_WORK: scd0, scd1, then the Event_init offset table up to scd0. Rooms
    // without room events have only the two script offsets (header 8).
    const auto header=v.u32(base_);if(header<8||header%4||header>4096)throw std::runtime_error("invalid event table");
    v.require(base_,header);scd0_=v.relative(base_,header,1);scd1_=v.relative(base_,v.u32(base_+4),1);
    for(std::size_t p=8;p<header;p+=4)scripts_.push_back(v.relative(base_,v.u32(base_+p),1));
    messages_=v.u32(info.room_offset+0x38);v.require(messages_,4);
    const auto count=v.u32(info.room_offset+0x9c),offset=v.u32(info.room_offset+0x1c);if(count>4096)throw std::runtime_error("too many room interactions");v.require(offset,count*36);
    for(unsigned i=0;i<count;++i){const auto p=offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};for(unsigned j=0;j<4;++j)a.parameters[j]=v.u8(p+32+j);interactions.push_back(a);}
    const auto floor_count=v.u32(info.room_offset+0xa0),floor_offset=v.u32(info.room_offset+0x20);
    if(floor_count>4096)throw std::runtime_error("too many floor triggers");v.require(floor_offset,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};for(unsigned j=0;j<4;++j)a.parameters[j]=v.u8(p+32+j);floors_.push_back(a);}
    const auto item_count=v.u32(info.room_offset+0x90),item_offset=v.u32(info.room_offset+16);if(item_count>4096)throw std::runtime_error("too many room items");v.require(item_offset,item_count*36);for(unsigned i=0;i<item_count;++i)room_items_.push_back(v.u16(item_offset+i*36+4));
    values[23]=inventory_.game_mode;values[24]=inventory_.player;flags[6][0]=0xffffffff;flags[13][0]=1;flags[14][0]=0x40000000;if(inventory_.side_pack)flags[11][0]|=0x8000000;
}
bool EventVm::flag(unsigned matrix,unsigned index)const{if(!matrix||matrix>=flags.size()||index>=1024)throw std::runtime_error("event flag outside matrix");return bool(flags[matrix][index/32]&(0x80000000u>>(index%32)));}
void EventVm::set_flag(unsigned matrix,unsigned index,unsigned mode){if(!matrix||matrix>=flags.size()||index>=1024||mode>5)throw std::runtime_error("invalid event flag operation");auto mask=(mode<3?0x80000000u:0xffffffffu)>>(index%32);auto& word=flags[matrix][index/32];switch(mode%3){case 0:word|=mask;break;case 1:word&=~mask;break;case 2:word^=mask;break;}if(matrix==11)inventory_.side_pack=bool(flags[11][0]&0x8000000);}
void EventVm::start(unsigned level,unsigned event){if(event>=scripts_.size())throw std::runtime_error("event index outside table");if(level>=16){level=0;while(level<16&&tasks_[level].active)++level;if(level==16)return;}if(task_started)task_started(level);tasks_[level]={};tasks_[level].active=true;tasks_[level].level=level;tasks_[level].event=event;tasks_[level].pc=scripts_[event];dispatched.push_back(event);}
void EventVm::inherit_progress(const EventVm& previous){
    // Original global event/key/item/map/game flags outlive a ROM_WORK load.
    for(unsigned matrix:{1u,2u,3u,7u,8u,9u,11u,12u,15u,16u})flags[matrix]=previous.flags[matrix];
    values[18]=previous.values[18];values[25]=previous.values[25];values[22]=inventory_.slots()[0];
}
void EventVm::initialize(){Task scenario;scenario.pc=scd0_;run(scenario,true);flags[6][0]|=0x10;if(std::any_of(tasks_.begin(),tasks_.end(),[](const auto& task){return task.active;}))++event_timer;for(auto& task:tasks_)if(task.active)run(task);flags[6][0]&=~0x10u;}
void EventVm::world(Task& task,unsigned op,std::size_t length){
    BinaryView v(room_);v.require(task.pc,length);EventCommand c{op,task.level,task.event,task.pc-base_,{room_.begin()+task.pc,room_.begin()+task.pc+length}};
    c.loop_remaining=task.cnt2;c.loop_total=task.cnt3;
    if((!binding||!binding(c))&&reported_.insert({op,task.pc}).second)pending_bindings.push_back(c);
    task.pc+=length;
}
int EventVm::instruction(Task& t){
    BinaryView v(room_);const auto p=t.pc;const unsigned op=v.u8(p);auto advance=[&](std::size_t n){v.require(p,n);t.pc+=n;return 1;};
    switch(op){
    case 0:return 0;
    case 1:{const auto target=v.relative(p+2,v.u8(p+1),1);if(t.branches.size()>=32)throw std::runtime_error("event IF nesting limit");t.branches.push_back(target);return advance(2);}
    case 2:if(t.branches.empty())throw std::runtime_error("event ELSE without IF");t.branches.pop_back();t.pc=v.relative(p,v.u8(p+1),1);return 1;
    case 3:if(t.branches.empty())throw std::runtime_error("event ENDIF without IF");t.branches.pop_back();return advance(2);
    case 4:{const unsigned matrix=v.u8(p+1),bit=v.u16(p+2),extra=v.u8(p+4),mode=v.u8(p+5);advance(6);if(matrix==10&&((bit==22&&values[16]!=extra)||(bit==23&&values[20]!=extra)))return 0;return int(mode)^int(flag(matrix,bit));}
    case 5:set_flag(v.u8(p+1),v.u16(p+2),v.u8(p+5));return advance(6);
    case 6:{const unsigned key=v.u8(p+1),mode=v.u8(p+2),operand=v.u8(p+3);advance(4);switch(mode){case 0:return values[key]==operand;case 1:return values[key]>operand;case 2:return values[key]>=operand;case 3:return values[key]<operand;case 4:return values[key]<=operand;case 5:return values[key]!=operand;default:throw std::runtime_error("unsupported event byte comparison");}}
    case 8:{const auto key=v.u8(p+1),value=v.u8(p+2);values[key]=value;if(key==22){if(value>=12)throw std::runtime_error("equipped inventory index outside slots");inventory_.slots()[0]=value;}return advance(4);}
    case 0xd:{if(enemy_dead&&enemy_dead(v.u8(p+1)))set_flag(3,v.u16(p+2),0);advance(4);return 1;}
    // bhEneSetCk: an enemy whose ed_flg bit is already set stays deactivated.
    case 0x22:{const unsigned enemy=v.u8(p+1),bit=v.u16(p+2)&0x1ff;advance(4);if(flag(3,bit)){if(!enemy_disable)throw std::runtime_error("enemy set check requires native world binding");enemy_disable(enemy);}return 1;}
    // bhCamCheck: only after an original skip (sp_flg 0x40) does an active
    // event camera v0 (and key v1 when v2) make the condition false.
    case 0x62:{const unsigned camera=v.u8(p+1),key=v.u8(p+2),match_key=v.u8(p+3);advance(6);if(!(flags[6][0]&0x40))return 1;
        if(flags[5][0]&1){unsigned active=0,active_key=0;if(!event_camera)throw std::runtime_error("camera check requires native world binding");if(event_camera(active,active_key)&&active==camera&&(!match_key||active_key==key))return 0;}return 1;}
    // bhCamInfoSet toggles CUT_WORK.flg bit 0 of camera-switch group v1.
    case 0x39:world(t,op,4);return 1;
    // Original Event_T_timer: one increment per scheduler pass with active work.
    case 0x60:{const auto limit=v.u16(p+4);advance(6);return event_timer<limit;}
    case 0x61:event_timer=0;return advance(2);
    case 0xd0:world(t,op,4);return 1;
    case 0x79:world(t,op,6);return 1;
    case 0x7a:world(t,op,2);return 1;
    case 0x78:world(t,op,14);return 1;
    case 0xba:world(t,op,16);return 1;
    case 0xbb:world(t,op,2);return 0;
    case 0x80:case 0x7e:world(t,op,4);return 1;
    case 0x4f:world(t,op,6);return 1;
    case 0x88:case 0xd2:world(t,op,10);return 1;
    case 0x45:world(t,op,2);return 1;
    case 0x66:{const unsigned command=v.u8(p+1);if(command!=1&&command!=20)throw std::runtime_error("unimplemented Obj_controll command "+std::to_string(command));world(t,op,command==1?2:4);return 1;}

    case 0xb9:world(t,op,10);return 1;
    // bhAreaSearchObj: X/Z minima inclusive, maxima exclusive; signed
    // magnitudes use separate masks. Byte 8 is padding, not the second mask.
    case 0x72:{v.require(p,14);if(!object_position)throw std::runtime_error("object area check requires native world binding");const unsigned kind=v.u8(p+2);if(kind>3)throw std::runtime_error("object area check kind outside original table");const auto position=object_position(kind,v.u8(p+1));if(!position)throw std::runtime_error("object area check entity unavailable");
        const auto first=v.u8(p+3),second=v.u8(p+9);const float x1=v.u16(p+4)/100.f*(first&1?-1:1),z1=v.u16(p+6)/100.f*(first&4?-1:1),x2=v.u16(p+10)/100.f*(second&1?-1:1),z2=v.u16(p+12)/100.f*(second&4?-1:1);advance(14);return x1<=position->x&&position->x<x2&&z1<=position->z&&position->z<z2;}
    // bhDieEventCk: compare the number of set ed_flg bits base..base+count-1.
    case 0x21:{const unsigned count=v.u8(p+1),base=v.u16(p+2),mode=v.u8(p+4),operand=v.u8(p+5);advance(6);unsigned dead=0;for(unsigned i=0;i<count;++i)dead+=flag(3,(base+i)&0x1ff);
        switch(mode){case 0:return dead==operand;case 1:return dead>operand;case 2:return dead>=operand;case 3:return dead<operand;case 4:return dead<=operand;case 5:return dead!=operand;default:return 0;}}
    case 0xb:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");if(v.u8(p+2))interactions[index].flags&=~1u;else interactions[index].flags|=1;return advance(4);}
    case 0xc:{const auto index=v.u8(p+1);if(index>=floors_.size())throw std::runtime_error("script floor outside room");if(v.u8(p+2))floors_[index].flags&=~1u;else floors_[index].flags|=1;return advance(4);}
    case 0x38:{const auto kind=v.u8(p+1),index=v.u8(p+2),half=v.u8(p+3);const auto target=unsigned(v.u16(p+4))*65536u+(half?32768u:0);if(!animation_frame)throw std::runtime_error("frame condition requires native actor binding");advance(6);return animation_frame(kind,index)<target;}
    case 0x1b:{const auto slot=v.u8(p+2);if(!audio_active)throw std::runtime_error("audio condition requires native mixer binding");advance(6);return audio_active(slot);}
    case 0x9b:if(!movie_active)throw std::runtime_error("movie condition requires native player binding");advance(6);return movie_active();
    case 0xe:{const unsigned bit=v.u16(p+2),interaction=v.u8(p+4),mode=v.u8(p+5);advance(6);if(values[20]!=interaction)return 1;
        if(interaction>=interactions.size())throw std::runtime_error("pickup interaction outside room");
        if(flags[10][0]&0x800){if(!flag(7,bit)){if(!mode)interactions[interaction].flags&=~1u;hide_item(interactions[interaction].parameters[0]);set_flag(7,bit,0);set_flag(9,bit,1);}}
        else if(!flag(7,bit))set_flag(9,bit,0);flags[10][0]&=~0x800u;return 1;}
    case 0x23:{const unsigned item=v.u8(p+1),bit=v.u16(p+2),interaction=v.u8(p+4),mode=v.u8(p+5);advance(6);
        if(interaction>=interactions.size())throw std::runtime_error("pickup interaction outside room");if(flag(7,bit)){if(!mode)interactions[interaction].flags&=~1u;hide_item(item);}else interactions[interaction].flags|=1;return 1;}
    case 0xf:flags[10][0]&=~0x400u;advance(2);return 0;
    case 0x10:{const auto item=v.u8(p+1);advance(2);return bool(flags[10][0]&0x400)&&values[8]==item;}
    case 0x11:{const auto item=v.u8(p+1);advance(2);return inventory_.contains(item);}
    case 0x12:{const auto mode=v.u8(p+1);advance(2);
        switch(mode){
        case 0:case 5:flags[10][0]=(flags[10][0]&~0x40u)|4;flags[5][0]|=4;break;
        case 1:flags[10][0]&=~4u;flags[5][0]&=~4u;flags[6][0]=0xffffffff;break;
        case 2:flags[10][0]&=~4u;flags[5][0]&=~4u;break;
        case 3:flags[10][0]|=0x44;flags[5][0]|=4;break;
        case 4:flags[10][0]&=~0x44u;flags[5][0]&=~4u;flags[6][0]=0xffffffff;break;
        default:throw std::runtime_error("invalid CineSet mode");}return 1;}
    case 0x63:{const auto limit=v.u8(p+1);if(!limit)throw std::runtime_error("random modulus zero");random_=random_*1664525u+1013904223u;values[17]=unsigned((std::uint64_t(random_)*100)>>32)%limit;return advance(2);}
    case 0x14:{const auto packed=v.u16(p+2);advance(4);start(packed&255,packed>>8);return 1;}
    case 0x1f:{const auto mode=v.u8(p+1),index=v.u8(p+2),freeze=v.u8(p+3);advance(4);if(!mode){flags[10][0]&=~0x2000u;dialog_.show(read_message(room_,messages_,index),index);flags[5][0]|=0x200;opening_hint_=t.event==16&&index==4;if(!freeze)flags[6][0]&=~7u;}else flags[6][0]|=7;return 1;}
    case 0x25:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");auto& a=interactions[index];a.attribute=v.u16(p+2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(p+4+i);a.type=v.u8(p+8);return advance(10);}
    case 0x7c:{const auto index=v.u8(p+1);if(index>=floors_.size())throw std::runtime_error("script floor outside room");auto& a=floors_[index];a.attribute=v.u16(p+2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(p+4+i);a.type=v.u8(p+8);return advance(10);}
    case 0x26:{const auto weapon=v.u8(p+1);advance(2);return inventory_.weapon()==weapon;}
    case 0x27:values[18]=v.u8(p+1);if(!values[18])inventory_.slots()[0]=0;return advance(2);
    case 0x28:status_requested=true;flags[12][0]|=0x200;return advance(2);
    case 0x31:{const auto item=v.u8(p+1);for(unsigned i=1;i<12;++i)if(Inventory::id(inventory_.slots()[i])==item){inventory_.consume(i);break;}return advance(2);}
    case 0x5c:dialog_=MessageDialog{};flags[5][0]&=~0x200u;return advance(2);
    case 0x81:{const auto mode=v.u8(p+1);advance(2);return mode==unsigned(!menu_open);}
    case 0xbc:{const auto level=v.u8(p+1);if(level>=16)throw std::runtime_error("event kill outside task table");advance(2);tasks_[level].active=false;return 1;}
    case 0xf8:t.delay=v.u16(p+2);if(!t.delay)t.delay=0xffffffff;t.pc++;return instruction(t);
    case 0xf9:if(t.delay>0)--t.delay;if(!t.delay)t.pc+=3;return 2;
    case 0xfe:t.pc++;return 2;
    case 0xf4:t.pc++;return 1;
    case 0xff:t.active=false;advance(2);return 2;
    case 0xfc:{if(t.loops.size()>=32)throw std::runtime_error("event WHILE nesting limit");auto end=v.relative(p,v.u8(p+1),1);t.loops.push_back({p+2,end,0,false});t.pc=end;return 1;}
    case 0xfd:case 0xf3:{if(t.loops.empty()||t.loops.back().repeat)throw std::runtime_error("event EWHILE without WHILE");const auto loop=t.loops.back();t.pc=loop.condition;const auto condition=instruction(t);if(condition==1){t.pc=loop.body;return 1;}t.pc=p+1;t.loops.pop_back();return op==0xf3?1:2;}
    case 0xfa:if(t.loops.size()>=32)throw std::runtime_error("event FOR nesting limit");t.cnt2=t.cnt3=v.u16(p+2);t.loops.push_back({0,p+4,t.cnt2,true});return advance(4);
    case 0xfb:if(t.loops.empty()||!t.loops.back().repeat)throw std::runtime_error("event NEXT without FOR");if(t.loops.back().count>1){t.cnt2=--t.loops.back().count;t.pc=t.loops.back().body;}else{t.loops.pop_back();advance(2);}return 1;
    // The binary structure is known, but these commands still need native
    // world/audio/cutscene bindings; keep them available for the next port step.
    case 0xad:case 0xd4:case 0x94:case 0xa:case 0x13:case 0x3d:case 0x3f:case 0x20:case 0x24:case 0x2f:case 0x35:case 0x3c:case 0x3e:case 0x43:case 0x4b:case 0x65:case 0x91:case 0x9a:case 0xa6:world(t,op,4);return 1;
    case 0x75:world(t,op,4);return 1;
    case 0x8b:world(t,op,12);return 1;
    case 0x8e:case 0x5a:case 0x1d:world(t,op,4);return 1;
    case 0x9f:case 0x50:world(t,op,2);return 1;
    case 0x6c:world(t,op,8);return 1;
    case 0x49:world(t,op,10);return 1;
    case 0x4a:world(t,op,8);return 1;
    case 0x1c:world(t,op,6);return 1;
    case 0x6e:world(t,op,22);return 1;
    case 0x6d:world(t,op,22);return 1; // bhCyodanSet: particle effect remains pending.
    case 0xab:world(t,op,6);return 1; // bhBGColorSet, including deferred application.
    case 0xd1:world(t,op,10);return 1;
    case 0x17:world(t,op,8);return 1;
    case 0x2d:case 0x2e:world(t,op,2);return 1;
    case 0x30:world(t,op,4);return 1;
    case 0x48:world(t,op,6);return 1;
    case 0x86:world(t,op,16);return 1;
    case 0x4c:case 0x44:world(t,op,6);return 1;
    case 0x33:{v.require(p,8);DoorRequest request{v.u16(p+2),v.u8(p+4),v.u8(p+5),v.u8(p+6),v.u8(p+7),values[21]};if(!door||!door(request))throw std::runtime_error("DoorCall destination unavailable");advance(8);return 2;}
    case 0x32:case 0x34:case 0x51:case 0x52:case 0x53:case 0x54:case 0x59:case 0xa3:world(t,op,12);return 1;
    case 0x3b:world(t,op,6);return 1;
    case 0x36:world(t,op,6);return 1;
    case 0x6a:if(!v.u8(p+1))flags[11][0]|=0x40000000u;else flags[11][0]&=~0x40000000u;return advance(2);
    case 0x16:case 0x18:case 0x1a:case 0x2b:case 0x42:case 0x5b:case 0x5f:case 0x29:case 0x9c:case 0x9e:world(t,op,op==0x42?4:2);return 1;
    // bhFaceReSet consumes one enemy index. Retain it as a pending facial
    // animation binding so the original scene cleanup task can continue.
    case 0x90:world(t,op,2);return 1;
    case 0xca:world(t,op,2);return 1; // bhEffClearEvt; event effects remain pending.
    case 0x5e:world(t,op,4);return 1;
    case 0x95:case 0xa7:world(t,op,4);return 1; // bhBgmOn2/bhBgmOn2Ex
    case 0x93:world(t,op,2);return 1;            // bhBgmOff2
    case 0x19:world(t,op,8);return 1;
    case 0x64:{unsigned length=0;switch(v.u8(p+1)){case 7:length=8;break;case 12:case 13:length=6;break;case 0x80:case 0x34:case 0x8b:case 0:length=2;break;case 0x83:case 0x89:length=4;break;default:throw std::runtime_error("unimplemented Player_controll command "+std::to_string(v.u8(p+1)));}world(t,op,length);return 1;}
    case 0x67:{unsigned length=0;switch(v.u8(p+1)){case 0:case 0x80:case 0x8b:case 0x90:length=2;break;case 0x8f:length=6;break;case 0x92:case 0x93:length=8;break;case 0x94:case 0x95:length=4;break;default:throw std::runtime_error("unimplemented Sub_controll command "+std::to_string(v.u8(p+1)));}world(t,op,length);return 1;}
    case 0x15:world(t,op,4);return 1;
    case 0x69:{const unsigned command=v.u8(p+1);unsigned length=0;switch(command){case 2:case 3:case 4:case 17:case 18:case 19:case 28:case 29:case 30:case 31:case 32:case 33:length=3;break;case 5:case 6:length=6;break;case 7:case 8:case 27:length=8;break;case 9:case 11:case 12:case 13:length=6;break;case 23:case 40:case 41:case 42:case 46:case 48:case 49:length=4;break;case 44:case 45:length=5;break;case 36:case 47:length=2;break;case 24:case 25:length=8;break;case 34:case 26:case 38:length=10;break;case 50:length=4;break;default:throw std::runtime_error("unimplemented Common_controll command "+std::to_string(command));}world(t,op,length);return 1;}
    default:{std::ostringstream s;s<<"unimplemented opcode 0x"<<std::hex<<op;throw std::runtime_error(s.str());}
    }
}
void EventVm::run(Task& t,bool scenario){
    if(t.blocked)return;try{for(unsigned count=0;count<10000;++count){const auto result=instruction(t);if(result==1)continue;if(result==0&&!t.branches.empty()){t.pc=t.branches.back();t.branches.pop_back();continue;}if(scenario)t.active=false;return;}throw std::runtime_error("event instruction budget exceeded");}
    catch(const std::exception& e){t.blocked=true;std::ostringstream s;s<<"event "<<t.event<<", task "<<t.level<<", offset 0x"<<std::hex<<(t.pc-base_)<<": "<<e.what();if(std::find(errors.begin(),errors.end(),s.str())==errors.end())errors.push_back(s.str());}
}
void EventVm::tick(){++tick_count;if(dialog_.tick()){flags[10][0]|=0x2000;if(opening_hint_)status_requested=true;opening_hint_=false;pickup_display_=0;}menu_open?flags[5][0]|=8:flags[5][0]&=~8u;if(dialog_.active())flags[5][0]|=0x200;else flags[5][0]&=~0x200u;Task scenario;scenario.pc=scd1_;run(scenario,true);if(std::any_of(tasks_.begin(),tasks_.end(),[](const auto& task){return task.active;}))++event_timer;for(auto& t:tasks_)if(t.active)run(t);}
void EventVm::hide_item(unsigned index){if(index>=room_items_.size())throw std::runtime_error("pickup model outside room");EventCommand command{0x20,0,0,0,{0x20,std::uint8_t(index),3,0}};if(!binding||!binding(command))pending_bindings.push_back(command);}
std::vector<EventTaskState> EventVm::task_states()const{std::vector<EventTaskState> out;for(const auto& t:tasks_)if(t.active)out.push_back({t.event,t.level,t.delay,t.pc-base_,t.blocked});return out;}
void EventVm::choose_dialog(int direction){const auto count=dialog_.choices();if(count)dialog_.selection=(dialog_.selection+count+direction)%count;}
void EventVm::cancel_dialog(){if(pickup_item_&&dialog_.choices()){dialog_.selection=1;confirm_dialog();}else confirm_dialog();}
void EventVm::confirm_dialog(){
    if(pickup_item_&&dialog_.choices()){
        const auto item=pickup_item_;pickup_item_=0;
        if(dialog_.selection){dialog_=MessageDialog{};pickup_display_=0;flags[5][0]&=~0x200u;flags[10][0]|=0x2000;return;}
        const auto count=getbulletmax[item][inventory_.game_mode];
        if(!inventory_.acquire(item,count)){dialog_.show(item_message(154,item),154);return;}
        flags[10][0]|=0x800;hide_item(pickup_model_);dialog_.show(item_message(158,item),158);return;
    }
    const bool was_active=dialog_.active();dialog_.confirm();if(was_active&&!dialog_.active()&&audio_stop)audio_stop(1);
    if(!dialog_.active()){pickup_display_=0;flags[5][0]&=~0x200u;flags[10][0]|=0x2000;if(opening_hint_)status_requested=true;opening_hint_=false;}
}
bool EventVm::use(unsigned item,unsigned* message){
    if(message)*message=0;if(!item||item>=156||!inventory_.contains(item))return false;
    const auto type=itemdata[item].type&0x5f;const Interaction* area=nullptr;
    if((flags[10][0]&0x200)&&values[16]<floors_.size())area=&floors_[values[16]];
    if((type==1||type==2||type==0x40)&&area&&std::find(area->parameters.begin(),area->parameters.end(),item)!=area->parameters.end()){
        if(item==82&&!Inventory::quantity(*std::find_if(inventory_.slots()+1,inventory_.slots()+12,[&](auto value){return Inventory::id(value)==item;}))&&values[0]==9&&values[1]==8){if(message)*message=195;return false;}
        auto* slot=std::find_if(inventory_.slots()+1,inventory_.slots()+12,[&](auto value){return Inventory::id(value)==item;});
        if(item==82&&Inventory::quantity(*slot))--*slot; // Keep the empty extinguisher, as ItemUse does.
        if(item==4)inventory_.items[224]=Inventory::quantity(*slot)&255;
        values[8]=item;flags[10][0]|=0x400;return true;}
    unsigned index=161;if(type==2)index=(item==18||item==36)?197:160;
    if(area&&(area->parameters[0]==59||area->parameters[0]==85)){index=174;if(type!=2){if(area->parameters[0]==59&&item==85)index=175;else if(area->parameters[0]==85&&item==59)index=176;}}
    if(message)*message=index;return false;
}
void EventVm::check_floor(NJS_POINT3 pos,Angle yaw,int floor){
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
bool EventVm::skip_cinematic(){if(!(flags[10][0]&0x40))return false;flags[10][0]|=0x10000000;return true;}
bool EventVm::interact(NJS_POINT3 pos,Angle yaw,int floor){
    if((flags[5][0]&4)||dialog_.active()||menu_open)return false;
    const auto direction=(yaw+8192)&0xc000;
    for(unsigned i=0;i<interactions.size();++i){const auto& a=interactions[i];const float distance=a.type==0?4.5f:2.0f;const auto probe=NJS_POINT3{pos.x-distance*njSin(yaw),pos.y,pos.z-distance*njCos(yaw)};if(!(a.flags&1)||probe.x<a.position.x||probe.x>a.position.x+a.extent.x||probe.z<a.position.z||probe.z>a.position.z+a.extent.z)continue;
        if(a.type==0){if(a.floor!=floor||(direction==32768&&(a.attribute&0x400))||(direction==16384&&(a.attribute&0x800))||(direction==0&&(a.attribute&0x1000))||(direction==49152&&(a.attribute&0x2000)))continue;
            return door&&door(DoorRequest{a.attribute,a.parameters[0],a.parameters[1],a.parameters[2],a.parameters[3],values[21]});}
        if(a.type==4){const auto model=a.parameters[0];if(model>=room_items_.size())throw std::runtime_error("pickup model outside room");auto id=room_items_[model];if(id==130)id=35;if(!id||id>=156||!item_message)return false;
            pickup_display_=pickup_item_=id;pickup_model_=model;values[20]=i;flags[10][0]|=0x100;dialog_.show(item_message(157,id),157);flags[5][0]|=0x200;return true;}
        values[20]=i;flags[10][0]|=0x100;if(a.type==3&&(a.attribute&0x8000)){dialog_.show(read_message(room_,messages_,a.parameters[1]),a.parameters[1]);flags[5][0]|=0x200;return true;}
        return true;
    }return false;
}
}
