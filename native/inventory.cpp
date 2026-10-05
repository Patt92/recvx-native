#include "inventory.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
namespace recvx {
namespace original {
struct S_WORK {unsigned int* gb=nullptr;unsigned int* pip=nullptr;unsigned listcsr_0=0,listcsr_1=0;unsigned statusflg=0;};
struct System {unsigned gm_mode=0,st_flg=0;};
static thread_local S_WORK swork;
static thread_local System system;
static thread_local System* sys=&system;
static thread_local unsigned message=0;
static void bhSetMessage(int,unsigned index){message=index;sys->st_flg|=0x200;}
#include "combinations.inc"
}
Inventory::Inventory(){
    // AllItemInit, normal new game, no bonus unlocks.
    items[1]=0x370001;items[2]=0x910001;
    items[18]=0x80001;items[19]=0x50012;items[20]=0xc001e;items[21]=0x140001;
    items[32]=2;items[33]=0x8a0001;items[34]=0x220190;items[383]=1;
}
unsigned Inventory::occupied()const{unsigned n=0;for(unsigned i=2;i<12;++i)if(auto item=id(slots()[i]))n+=(itemdata[item].type&0x100)?2:1;return n;}
bool Inventory::contains(unsigned item)const{for(unsigned i=1;i<12;++i)if(id(slots()[i])==item)return true;return false;}
unsigned Inventory::weapon()const{
    if(!slots()[0]||slots()[0]>=12)return 0;
    const auto value=slots()[slots()[0]],item=id(value),flags=(value>>16)&0x7000;
    switch(item){case 1:return 20;case 2:return 12;case 3:return 13;case 4:return 11;case 5:case 131:return 3;
    case 6:return flags==0x1000?15:(flags==0x2000?16:(flags==0x4000?17:14));case 7:return flags==0x2000?19:10;
    case 8:return 2;case 9:return 4;case 10:return 5;case 11:return 18;case 32:return 6;case 33:return 7;case 34:return 8;case 55:return 1;case 142:return 9;default:return 0;}
}
bool Inventory::equip(unsigned slot){if(!slot||slot>=12||!id(slots()[slot])||!(itemdata[id(slots()[slot])].type&1))return false;slots()[0]=slots()[0]==slot?0:slot;return true;}
void Inventory::compact(unsigned* tracked_slot){
    // Original ItemSort: large weapons first, retaining equipment identity.
    std::array<std::tuple<std::uint32_t,bool,unsigned>,10> list{};unsigned n=0;const auto tracked=tracked_slot?*tracked_slot:0;
    for(unsigned large=1;;--large){for(unsigned i=2;i<12;++i){auto item=id(slots()[i]);if(item&&bool(itemdata[item].type&0x100)==bool(large))list[n++]={slots()[i],slots()[0]==i,i};}if(!large)break;}
    std::fill(slots()+2,slots()+12,0);if(slots()[0]!=1)slots()[0]=0;
    for(unsigned i=0;i<n;++i){slots()[i+2]=std::get<0>(list[i]);if(std::get<1>(list[i]))slots()[0]=i+2;if(tracked_slot&&std::get<2>(list[i])==tracked)*tracked_slot=i+2;}
}
bool Inventory::acquire(unsigned item,unsigned count,unsigned flags){
    if(!item||item>=156||count>65535||flags&~0xff00u)return false;
    // Original unloaded firearms and knives are valid pickups with zero rounds.
    if(!count){if(!(itemdata[item].type&1)||occupied()+((itemdata[item].type&0x100)?2:1)>capacity())return false;unsigned slot=2;while(slot<12&&id(slots()[slot]))++slot;if(slot==12)return false;slots()[slot]=pack(item|flags,0);compact();return true;}
    auto saved=items;
    // Stack ammo using the original per-item maxima, never partially accepting a pickup.
    if(itemdata[item].type&0x200)for(unsigned i=2;i<12&&count;++i)if((slots()[i]>>16)==(item|flags)){
        const auto max=item==42?300u:999u,used=quantity(slots()[i]);const auto take=std::min(count,max-std::min(max,used));slots()[i]+=take;count-=take;
    }
    while(count){if(occupied()+((itemdata[item].type&0x100)?2:1)>capacity()){items=saved;return false;}
        unsigned slot=2;while(slot<12&&id(slots()[slot]))++slot;if(slot==12){items=saved;return false;}
        unsigned take=count;if(itemdata[item].type&0x200)take=std::min(count,item==42?300u:999u);
        else take=std::min(count,std::max(1u,unsigned(itemdata[item].max)));
        slots()[slot]=pack(item|flags,take);count-=take;
    }
    compact();return true;
}
bool Inventory::consume(unsigned slot){if(slot<1||slot>=12||!id(slots()[slot]))return false;if(quantity(slots()[slot])>1)--slots()[slot];else{slots()[slot]=0;if(slots()[0]==slot)slots()[0]=0;}compact();return true;}
bool Inventory::heal(unsigned slot){
    if(slot<1||slot>=12)return false;const auto item=id(slots()[slot]);if(item<20||item>29)return false;
    // TrigerSet uses curedata for herbs/spray. Blue herbs only cure poison.
    const auto cure=curedata[item-20];const auto heal=cure&15;const bool poison_cure=(cure&0xf0)==0x10;
    if(!heal&&!(poison_cure&&poisoned))return false;
    if(poison_cure)poisoned=false;
    if(heal>=3)hp=max_hp;else hp=std::min(max_hp,hp+int(heal)*50*(game_mode==2?2:1));return consume(slot);
}
std::string Inventory::condition()const{return poisoned?"Poison":hp>=120?"Fine":hp>=60?"Caution":hp>=30?"Caution":"Danger";}
bool Inventory::combine(unsigned first,unsigned second,unsigned* out,unsigned* result_slot){
    if(out)*out=0;if(result_slot)*result_slot=0;
    if(first<1||second<1||first>=12||second>=12||first==second)return false;
    auto effective=[](std::uint32_t v){auto item=id(v);const auto flags=(v>>16)&0x7000;if(item==6)return flags==0x1000?152u:(flags==0x2000?153u:(flags==0x4000?154u:6u));if(item==7&&flags==0x2000)return 155u;return item;};
    const auto a=effective(slots()[first]),b=effective(slots()[second]);if(!a||!b||a>=156||b>=156||itemdata[b].cmb<0)return false;
    unsigned pos=unsigned(itemdata[b].cmb),count=unsigned(combidata[pos++]);bool found=false;
    for(unsigned i=0;i<count&&pos+2<410;++i,pos+=3)if(unsigned(combidata[pos])==a){found=true;break;}
    if(!found)return false;const auto op=unsigned(combidata[pos+2])&255;
    auto saved=items;original::swork={nullptr,slots(),first,second,0};original::system={game_mode,0};original::message=0;
    auto* from=&slots()[first];auto* to=&slots()[second];if(op==1||op==4||op==5||op==8||op==11)std::swap(from,to);
    unsigned result=0;switch(op){case 0:case 1:result=original::Combi_00(short(pos),from,to);break;
    case 2:case 5:result=original::Combi_01(short(pos),from,to);break;
    case 3:case 4:result=original::Combi_02(short(pos),from,to);break;
    case 6:case 9:result=original::Combi_03(short(pos),from,to);break;
    case 7:case 8:result=original::Combi_04(short(pos),from,to);break;
    case 10:case 11:if(quantity(*from)<1||quantity(*from)>4){items=saved;return false;}result=original::Combi_05(short(pos),from,to);break;
    default:items=saved;return false;}
    if(out)*out=original::message;
    // Combi_04 can successfully convert part of a stack while returning zero.
    // The return bits drive original cursor animation; the actual mutation and
    // message determine whether this transactional inventory change succeeds.
    (void)result;if(original::message||items==saved||occupied()>capacity()){items=saved;return false;}
    const auto product=unsigned(combidata[pos+1])&255;
    unsigned destination=id(*to)==product?unsigned(to-slots()):id(*from)==product?unsigned(from-slots()):id(*to)?unsigned(to-slots()):unsigned(from-slots());
    compact(&destination);if(result_slot)*result_slot=destination;return true;
}
bool Inventory::transform(unsigned slot,unsigned expected,unsigned replacement){
    if(slot<1||slot>=12||replacement>=156||id(slots()[slot])!=expected)return false;const auto saved=items;
    slots()[slot]=replacement?pack(replacement,itemdata[replacement].max):0;
    if(slots()[0]==slot&&(!replacement||!(itemdata[replacement].type&1)))slots()[0]=0;
    if(occupied()>capacity()){items=saved;return false;}return true;
}
bool Inventory::deposit(unsigned slot,unsigned box){if(slot<2||slot>=12||box!=64||!id(slots()[slot]))return false;unsigned i=0;while(i<128&&items[box+i])++i;if(i==128)return false;items[box+i]=slots()[slot];slots()[slot]=0;if(slots()[0]==slot)slots()[0]=0;compact();return true;}
bool Inventory::withdraw(unsigned index,unsigned box){if(box!=64||index>=128||!items[box+index])return false;auto item=items[box+index];if(!acquire(id(item),quantity(item),(item>>16)&0xff00))return false;items[box+index]=0;return true;}
}
