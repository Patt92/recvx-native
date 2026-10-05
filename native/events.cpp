#include "events.h"
#include "binary_view.h"
#include "room.h"
#include "ninja_math.h"
#include <algorithm>
#include <sstream>
namespace recvx {
EventVm::EventVm(const std::vector<std::uint8_t>& room,Inventory& inv,MessageDialog& dialog):room_(room),inventory_(inv),dialog_(dialog){
    BinaryView v(room_);const auto info=inspect_room(room_);base_=info.event_offset;if(!base_)throw std::runtime_error("room event section missing");
    const auto header=v.u32(base_);if(header<12||header%4||header>4096)throw std::runtime_error("invalid event table");
    v.require(base_,header);scd0_=v.relative(base_,header,1);scd1_=v.relative(base_,v.u32(base_+4),1);
    for(std::size_t p=8;p<header;p+=4)scripts_.push_back(v.relative(base_,v.u32(base_+p),1));
    messages_=v.u32(info.room_offset+0x38);v.require(messages_,4);
    const auto count=v.u32(info.room_offset+0x9c),offset=v.u32(info.room_offset+0x1c);if(count>4096)throw std::runtime_error("too many room interactions");v.require(offset,count*36);
    for(unsigned i=0;i<count;++i){const auto p=offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};for(unsigned j=0;j<4;++j)a.parameters[j]=v.u8(p+32+j);interactions.push_back(a);}
    const auto floor_count=v.u32(info.room_offset+0xa0),floor_offset=v.u32(info.room_offset+0x20);
    if(floor_count>4096)throw std::runtime_error("too many floor triggers");v.require(floor_offset,floor_count*36);
    for(unsigned i=0;i<floor_count;++i){const auto p=floor_offset+i*36;Interaction a;a.flags=v.u8(p);a.type=v.u8(p+1);a.floor=std::int8_t(v.u8(p+3));a.attribute=v.u32(p+4);a.position={v.f32(p+8),v.f32(p+12),v.f32(p+16)};a.extent={v.f32(p+20),v.f32(p+24),v.f32(p+28)};floors_.push_back(a);}
    values[23]=inventory_.game_mode;values[24]=inventory_.player;flags[6][0]=0xffffffff;flags[13][0]=1;flags[14][0]=0x40000000;if(inventory_.side_pack)flags[11][0]|=0x8000000;
}
bool EventVm::flag(unsigned matrix,unsigned index)const{if(!matrix||matrix>=flags.size()||index>=1024)throw std::runtime_error("event flag outside matrix");return bool(flags[matrix][index/32]&(0x80000000u>>(index%32)));}
void EventVm::set_flag(unsigned matrix,unsigned index,unsigned mode){if(!matrix||matrix>=flags.size()||index>=1024||mode>5)throw std::runtime_error("invalid event flag operation");auto mask=(mode<3?0x80000000u:0xffffffffu)>>(index%32);auto& word=flags[matrix][index/32];switch(mode%3){case 0:word|=mask;break;case 1:word&=~mask;break;case 2:word^=mask;break;}if(matrix==11)inventory_.side_pack=bool(flags[11][0]&0x8000000);}
void EventVm::start(unsigned level,unsigned event){if(event>=scripts_.size())throw std::runtime_error("event index outside table");if(level>=16){level=0;while(level<16&&tasks_[level].active)++level;if(level==16)return;}tasks_[level]={};tasks_[level].active=true;tasks_[level].level=level;tasks_[level].event=event;tasks_[level].pc=scripts_[event];dispatched.push_back(event);}
void EventVm::inherit_progress(const EventVm& previous){
    // Original global event/key/item/map/game flags outlive a ROM_WORK load.
    for(unsigned matrix:{1u,2u,3u,7u,8u,9u,11u,12u,15u,16u})flags[matrix]=previous.flags[matrix];
    values[18]=previous.values[18];values[25]=previous.values[25];values[22]=inventory_.slots()[0];
}
void EventVm::initialize(){Task scenario;scenario.pc=scd0_;run(scenario,true);flags[6][0]|=0x10;for(auto& task:tasks_)if(task.active)run(task);flags[6][0]&=~0x10u;}
void EventVm::world(Task& task,unsigned op,std::size_t length){
    BinaryView v(room_);v.require(task.pc,length);EventCommand c{op,task.level,task.event,task.pc-base_,{room_.begin()+task.pc,room_.begin()+task.pc+length}};
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
    case 0xb:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");if(v.u8(p+2))interactions[index].flags&=~1u;else interactions[index].flags|=1;return advance(4);}
    case 0xc:{const auto index=v.u8(p+1);if(index>=floors_.size())throw std::runtime_error("script floor outside room");if(v.u8(p+2))floors_[index].flags&=~1u;else floors_[index].flags|=1;return advance(4);}
    case 0x38:{const auto kind=v.u8(p+1),index=v.u8(p+2),half=v.u8(p+3);const auto target=unsigned(v.u16(p+4))*65536u+(half?32768u:0);if(!animation_frame)throw std::runtime_error("frame condition requires native actor binding");advance(6);return animation_frame(kind,index)<target;}
    case 0x1b:{const auto slot=v.u8(p+2);if(!audio_active)throw std::runtime_error("audio condition requires native mixer binding");advance(6);return audio_active(slot);}
    case 0x9b:if(!movie_active)throw std::runtime_error("movie condition requires native player binding");advance(6);return movie_active();
    case 0xe:return advance(6); // Item visibility is governed by pickup flags in world binding.
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
    case 0x1f:{const auto mode=v.u8(p+1),index=v.u8(p+2),freeze=v.u8(p+3);advance(4);if(!mode){flags[10][0]&=~0x2000u;dialog_.show(read_message(room_,messages_,index),index);flags[5][0]|=0x200;if(!freeze)flags[6][0]&=~7u;}else flags[6][0]|=7;return 1;}
    case 0x25:{const auto index=v.u8(p+1);if(index>=interactions.size())throw std::runtime_error("script interaction outside room");auto& a=interactions[index];a.attribute=v.u16(p+2);for(unsigned i=0;i<4;++i)a.parameters[i]=v.u8(p+4+i);a.type=v.u8(p+8);return advance(10);}
    case 0x26:{const auto weapon=v.u8(p+1);advance(2);return inventory_.weapon()==weapon;}
    case 0x27:values[18]=v.u8(p+1);if(!values[18])inventory_.slots()[0]=0;return advance(2);
    case 0x28:flags[12][0]|=0x200;return advance(2);
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
    case 0xfa:if(t.loops.size()>=32)throw std::runtime_error("event FOR nesting limit");t.loops.push_back({0,p+4,v.u16(p+2),true});return advance(4);
    case 0xfb:if(t.loops.empty()||!t.loops.back().repeat)throw std::runtime_error("event NEXT without FOR");if(t.loops.back().count>1){--t.loops.back().count;t.pc=t.loops.back().body;}else{t.loops.pop_back();advance(2);}return 1;
    // The binary structure is known, but these commands still need native
    // world/audio/cutscene bindings; keep them available for the next port step.
    case 0xa:case 0x13:case 0x3d:case 0x3f:case 0x20:case 0x24:case 0x2f:case 0x35:case 0x3c:case 0x3e:case 0x43:case 0x4b:case 0x65:case 0x91:case 0x9a:case 0xa6:world(t,op,4);return 1;
    case 0x86:world(t,op,16);return 1;
    case 0x23:case 0x44:world(t,op,6);return 1;
    case 0x33:{v.require(p,8);DoorRequest request{v.u16(p+2),v.u8(p+4),v.u8(p+5),v.u8(p+6),v.u8(p+7),values[21]};if(!door||!door(request))throw std::runtime_error("DoorCall destination unavailable");advance(8);return 2;}
    case 0x32:case 0x53:case 0x34:world(t,op,12);return 1;
    case 0x3b:world(t,op,6);return 1;
    case 0x36:world(t,op,6);return 1;
    case 0x16:case 0x18:case 0x1a:case 0x2b:case 0x42:case 0x5b:case 0x5f:case 0x29:case 0x9c:case 0x6a:case 0x9e:world(t,op,op==0x42?4:2);return 1;
    case 0x5e:world(t,op,4);return 1;
    case 0x19:world(t,op,8);return 1;
    case 0x64:{unsigned length=0;switch(v.u8(p+1)){case 7:length=8;break;case 12:case 13:length=6;break;case 0x80:case 0x34:case 0x8b:case 0:length=2;break;case 0x83:case 0x89:length=4;break;default:throw std::runtime_error("unimplemented Player_controll command "+std::to_string(v.u8(p+1)));}world(t,op,length);return 1;}
    case 0x67:{if(v.u8(p+1)!=0x80&&v.u8(p+1)!=0x8b)throw std::runtime_error("unimplemented Sub_controll command "+std::to_string(v.u8(p+1)));world(t,op,2);return 1;}
    case 0x15:world(t,op,4);return 1;
    case 0x69:{const unsigned command=v.u8(p+1);unsigned length=0;switch(command){case 2:case 3:case 4:length=4;break;case 5:case 6:length=6;break;case 7:case 8:length=8;break;case 9:case 11:case 12:case 13:length=6;break;case 23:case 41:length=4;break;case 36:case 47:length=2;break;case 24:case 25:length=8;break;case 34:length=10;break;default:throw std::runtime_error("unimplemented Common_controll command "+std::to_string(command));}world(t,op,length);return 1;}
    default:{std::ostringstream s;s<<"unimplemented opcode 0x"<<std::hex<<op;throw std::runtime_error(s.str());}
    }
}
void EventVm::run(Task& t,bool scenario){
    if(t.blocked)return;try{for(unsigned count=0;count<10000;++count){const auto result=instruction(t);if(result==1)continue;if(result==0&&!t.branches.empty()){t.pc=t.branches.back();t.branches.pop_back();continue;}if(scenario)t.active=false;return;}throw std::runtime_error("event instruction budget exceeded");}
    catch(const std::exception& e){t.blocked=true;std::ostringstream s;s<<"event "<<t.event<<", task "<<t.level<<", offset 0x"<<std::hex<<(t.pc-base_)<<": "<<e.what();if(std::find(errors.begin(),errors.end(),s.str())==errors.end())errors.push_back(s.str());}
}
void EventVm::tick(){++tick_count;if(dialog_.tick())flags[10][0]|=0x2000;menu_open?flags[5][0]|=8:flags[5][0]&=~8u;if(dialog_.active())flags[5][0]|=0x200;else flags[5][0]&=~0x200u;Task scenario;scenario.pc=scd1_;run(scenario,true);for(auto& t:tasks_)if(t.active)run(t);}
void EventVm::confirm_dialog(){dialog_.confirm();if(!dialog_.active()){flags[5][0]&=~0x200u;flags[10][0]|=0x2000;}}
void EventVm::use(unsigned item){values[8]=item;flags[10][0]|=0x400;}
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
    if(flags[5][0]&4)return false;
    const auto direction=(yaw+8192)&0xc000;
    for(unsigned i=0;i<interactions.size();++i){const auto& a=interactions[i];const float distance=a.type==0?4.5f:2.0f;const auto probe=NJS_POINT3{pos.x-distance*njSin(yaw),pos.y,pos.z-distance*njCos(yaw)};if(!(a.flags&1)||probe.x<a.position.x||probe.x>a.position.x+a.extent.x||probe.z<a.position.z||probe.z>a.position.z+a.extent.z)continue;
        if(a.type==0){if(a.floor!=floor||(direction==32768&&(a.attribute&0x400))||(direction==16384&&(a.attribute&0x800))||(direction==0&&(a.attribute&0x1000))||(direction==49152&&(a.attribute&0x2000)))continue;
            return door&&door(DoorRequest{a.attribute,a.parameters[0],a.parameters[1],a.parameters[2],a.parameters[3],values[21]});}
        values[20]=i;flags[10][0]|=0x100;if(a.type==3&&(a.attribute&0x8000)){dialog_.show(read_message(room_,messages_,a.parameters[1]),a.parameters[1]);flags[5][0]|=0x200;return true;}
        return true;
    }return false;
}
}
