#include "inventory.h"
#include "inspection.h"
#include "status_pulse.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
namespace recvx {
namespace original {
struct S_WORK {unsigned int* gb=nullptr;unsigned int* pip=nullptr;unsigned listcsr_0=0,listcsr_1=0;unsigned statusflg=0;unsigned int* bxp=nullptr;unsigned listcsr_2=0;};
struct System {unsigned gm_mode=0,st_flg=0,gm_flg=0,ply_id=0,stg_no=0,rom_no=0,mn_mode0=0;};
struct Player {unsigned mode3=0,wpnr_no=0;};
struct Part {unsigned atr=0,anim=0;};
static thread_local Player player;static thread_local Player* plp=&player;
static thread_local Part parts_14b[1],parts_15b[20];static thread_local unsigned sakai=2;
static thread_local S_WORK swork;
static thread_local System system;
static thread_local System* sys=&system;
static thread_local unsigned message=0;
static void bhSetMessage(int,unsigned index){message=index;sys->st_flg|=0x200;}
#include "combinations.inc"
#include "box_engine.inc"
}
namespace pickup_original {
struct S_WORK {unsigned int* gb=nullptr;unsigned int* pip=nullptr;unsigned testmode=0,statusflg=0,flgchk=0;unsigned char hp=0,abc=0;};
struct System {unsigned gm_mode=0,gm_flg=0,cb_flg=0,st_flg=0,pad_ps=0,mes_sel=0,sb_id=0,ply_id=0,rom_no=0,stg_no=0,mn_mode0=0;unsigned* itm=nullptr;short spray_ct=0;};
struct Player {unsigned mode3=0,wpnr_no=0,at_flg=0;int hp=0;unsigned stflg=0;};
struct PULSE_WORK {unsigned pulseflg=0;};
static thread_local S_WORK swork;
static thread_local System system;static thread_local System* sys=&system;
static thread_local Player player;static thread_local Player* plp=&player;
static thread_local unsigned message=0;
static thread_local PULSE_WORK pulsework;static thread_local bool pulse_init=false;
static void PulseInit(){pulsework={};pulse_init=true;}
static thread_local unsigned sakai=2;
static void bhSetMessage(int,unsigned index){message=index;sys->st_flg|=0x200;}
// Window/audio transitions are driven by the host status task. GetItem still
// decides which question, mutation and equipment transition to perform.
static void StatusCancel(S_WORK*){}
static void CallSystemSe(int,int){}
static void WeaponSet(unsigned slot,unsigned){swork.pip[0]=slot;}
#include "item_use00.inc"
#include "pickup_engine.inc"
}
namespace item_use_original {
struct S_WORK {unsigned* pip=nullptr;unsigned listcsr_0=0,itemid=0,statusflg=0,flgchk=0,testmode=0;unsigned char hp=0,abc=0;};
struct System {unsigned* itm=nullptr;unsigned ply_id=0,sb_id=0,cb_flg=0,st_flg=0,flr_idx=0,stg_no=0,rom_no=0;int gm_mode=0;short spray_ct=0;};
struct Player {int hp=0;unsigned stflg=0;};
struct PULSE_WORK {unsigned pulseflg=0;};
struct ATR_WORK {unsigned prm0=0,prm1=0,prm2=0,prm3=0;};
struct Room {ATR_WORK* flrp;};
static thread_local S_WORK swork;
static thread_local System system;static thread_local System* sys=&system;
static thread_local Player player;static thread_local Player* plp=&player;
static thread_local ATR_WORK area;static thread_local Room room{&area};static thread_local Room* rom=&room;
static thread_local unsigned message=0;static thread_local bool closing=false;
// PULSE_WORK belongs to the status screen (StatusPulse). Use_00 calls the
// original PulseInit and then sets pulseflg; the host replays exactly that
// sequence on the live monitor with the captured bits.
static thread_local PULSE_WORK pulsework;static thread_local bool pulse_init=false;
static void PulseInit(){pulsework={};pulse_init=true;}
static void bhSetMessage(int,unsigned index){message=index;sys->st_flg|=0x200;}
static void StatusCancel(S_WORK*){closing=true;}
#include "item_use00.inc"
#include "item_use.inc"
}
ItemUseResult Inventory::use_item(unsigned slot,const std::array<unsigned,4>* parameters,unsigned& control,unsigned stage,unsigned room_number,unsigned condition,bool message_active){
    if(player>=4||!slot||slot>=12||!id(slots()[slot])||id(slots()[slot])>=156||condition>4)throw std::runtime_error("invalid original ItemUse slot");
    using namespace item_use_original;
    item_use_original::system={};auto& sys=item_use_original::system;sys.itm=items.data();sys.ply_id=player;sys.cb_flg=control;sys.stg_no=stage;sys.rom_no=room_number;
    sys.gm_mode=int(game_mode);sys.spray_ct=spray_count;sys.st_flg=message_active?0x200u:0u;
    if(parameters){area={(*parameters)[0],(*parameters)[1],(*parameters)[2],(*parameters)[3]};sys.cb_flg|=0x200;}else sys.cb_flg&=~0x200u;
    item_use_original::player={hp,player_stflg()};
    // ItemCommand sets st->itemid from the cursor; StatusInit/Use_00 keep st->hp == st->abc.
    swork={};swork.pip=slots();swork.listcsr_0=slot;swork.itemid=id(slots()[slot]);swork.hp=swork.abc=std::uint8_t(condition);
    message=0;closing=false;pulsework={};pulse_init=false;
    const unsigned useon=ItemUse(&swork);
    hp=item_use_original::player.hp;set_player_stflg(item_use_original::player.stflg);spray_count=sys.spray_ct;
    // StatusMain clears cb_flg 0x400 on its next frame; only StatusCancel lets
    // the event VM observe the used item.
    control=closing?sys.cb_flg:(sys.cb_flg&~0x400u);
    return {closing,message,useon,pulse_init?pulsework.pulseflg:0u,swork.hp};
}
PickupResult Inventory::pickup(unsigned item,unsigned& control,unsigned stage,unsigned room,unsigned mode,int answer){
    if(!item||item>=156||player>=4||game_mode>=4||mode>5||answer < -1||answer>101)throw std::runtime_error("invalid original pickup state");
    using namespace pickup_original;
    pickup_original::system={};auto& sys=pickup_original::system;
    sys.gm_mode=game_mode;sys.gm_flg=side_pack?0x8000000:0;sys.cb_flg=control;sys.sb_id=item;sys.ply_id=player;sys.itm=items.data();sys.stg_no=stage;sys.rom_no=room;
    sys.spray_ct=spray_count;pickup_original::player={};pickup_original::player.hp=hp;pickup_original::player.stflg=player_stflg();
    swork={};swork.pip=slots();swork.testmode=mode;swork.hp=swork.abc=std::uint8_t(StatusPulse::condition(hp,(player_stflg()&0x280000u)!=0));message=0;pulsework={};pulse_init=false;
    // Host debug actions can alter inventory while a question is displayed.
    // Re-run the original capacity branch before its unchecked insertion loop.
    if(mode==1&&answer==100){sys.cb_flg&=~0x5800u;swork.testmode=0;GetItem(&swork);if(swork.testmode!=1){control=sys.cb_flg;return {swork.testmode,message,false,false};}message=0;}
    if(answer>=0){sys.pad_ps=0x800;sys.st_flg=0x4200;sys.cb_flg|=0x1000;sys.mes_sel=answer;}
    const unsigned before=sys.cb_flg;
    GetItem(&swork);
    const bool granted=swork.statusflg&0x100;
    // Only GetItem's immediate-use answer (mode 4, mes_sel 100) sets cb_flg 0x400 and calls Use_00.
    const bool used=(sys.cb_flg&~before)&0x400;
    hp=pickup_original::player.hp;set_player_stflg(pickup_original::player.stflg);spray_count=sys.spray_ct;
    // GetItem mode 2 emits the received-item message on its next pass after
    // the question has closed; preserve that same state transition.
    if(swork.testmode==2){GetItem(&swork);}
    // StatusMain calls SpriteOnOff after GetItem, which calls ItemSort before
    // ItemSet. GetItem has already adjusted the equipped narrow-item index for
    // a newly inserted wide item; do not remap that index a second time here.
    ItemSort();
    control=sys.cb_flg;
    return {swork.testmode,message,granted,used,pulse_init?pulsework.pulseflg:0u};
}
bool Inventory::reload(){
    if(!slots()[0]||slots()[0]>=12||!weapon())return false;
    pickup_original::swork={};pickup_original::swork.pip=slots();
    pickup_original::system={};pickup_original::system.gm_mode=game_mode;
    pickup_original::system.st_flg=side_pack?0x8000000u:0;
    return pickup_original::bhSearchBullet()!=0;
}
bool Inventory::count_bullet(){
    if(!slots()[0]||slots()[0]>=12||!weapon())return false;
    pickup_original::swork={};pickup_original::swork.pip=slots();
    pickup_original::player={};pickup_original::system={};
    return pickup_original::bhCountBullet()!=0;
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
    pickup_original::swork={};pickup_original::swork.pip=slots();pickup_original::ItemSort();
    if(slots()[0]!=1)slots()[0]=0;
    for(unsigned i=0;i<n;++i){if(std::get<1>(list[i]))slots()[0]=i+2;if(tracked_slot&&std::get<2>(list[i])==tracked)*tracked_slot=i+2;}
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
bool Inventory::debug_acquire(unsigned choice,unsigned count){
    if(choice>=256&&choice<280){items[383]|=1u<<(choice-256);return true;}
    if(!choice||choice>=156||original_file_model_mask(choice))return false;
    return acquire(choice,(itemdata[choice].type&1)&&!itemdata[choice].max?0:count);
}
unsigned Inventory::repair_owned_file_items(){
    const auto owner=player;unsigned removed=0;
    for(player=0;player<4;++player){bool changed=false;
        for(unsigned slot=1;slot<12;++slot){const auto item=id(slots()[slot]);
            if(original_file_model_mask(item)&items[383]){slots()[slot]=0;if(slots()[0]==slot)slots()[0]=0;++removed;changed=true;}}
        if(changed)compact();
    }
    player=owner;return removed;
}
void Inventory::erase_read_manual(bool automatic){
    pickup_original::swork={};pickup_original::swork.pip=slots();
    if(automatic){if(auto* item=pickup_original::ItemSearch(145))pickup_original::EraseItem(item);}
    else for(unsigned i=0;i<192;++i)if(items[i]==0x910001){items[i]=0;break;}
    // StatusMain -> SpriteOnOff calls ItemSort for the current player's list.
    pickup_original::ItemSort();
}
std::string Inventory::condition()const{return poisoned?"Poison":hp>=120?"Fine":hp>=60?"Caution":hp>=30?"Caution":"Danger";}
int Inventory::combination_rule(unsigned first,unsigned second)const{
    if(first<1||second<1||first>=12||second>=12||first==second)return -1;
    auto effective=[](std::uint32_t v){auto item=id(v);const auto flags=(v>>16)&0x7000;if(item==6)return flags==0x1000?152u:(flags==0x2000?153u:(flags==0x4000?154u:6u));if(item==7&&flags==0x2000)return 155u;return item;};
    const auto a=effective(slots()[first]),b=effective(slots()[second]);if(!a||!b||a>=156||b>=156||itemdata[b].cmb<0)return -1;
    unsigned pos=unsigned(itemdata[b].cmb),count=unsigned(combidata[pos++]);bool found=false;
    for(unsigned i=0;i<count&&pos+2<410;++i,pos+=3)if(unsigned(combidata[pos])==a){found=true;break;}
    if(!found)return -1;return int(pos);
}
bool Inventory::combine(unsigned first,unsigned second,unsigned* out,unsigned* result_slot){
    if(out)*out=0;if(result_slot)*result_slot=0;
    if(first<1||second<1||first>=12||second>=12||first==second)return false;
    const int rule=combination_rule(first,second);if(rule<0)return false;const auto pos=unsigned(rule),op=unsigned(combidata[pos+2])&255;
    auto saved=items;original::swork={nullptr,slots(),first,second,0};original::system={game_mode,0};original::message=0;
    auto* from=&slots()[first];auto* to=&slots()[second];if(op==1||op==4||op==5||op==8||op==11)std::swap(from,to);
    unsigned result=0;switch(op){case 0:case 1:result=original::Combi_00(short(pos),from,to);break;
    case 2:case 5:result=original::Combi_01(short(pos),from,to);break;
    case 3:case 4:result=original::Combi_02(short(pos),from,to);break;
    case 6:case 9:result=original::Combi_03(short(pos),from,to);break;
    case 7:case 8:result=original::Combi_04(short(pos),from,to);break;
    case 10:case 11:{const auto body=id(*to);const auto available=body==124?1u:body==123?2u:body==122?3u:4u;if(quantity(*from)<1||quantity(*from)>available){items=saved;return false;}result=original::Combi_05(short(pos),from,to);break;}
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
bool Inventory::box_change(unsigned slot,unsigned index,unsigned box,unsigned stage,unsigned room,unsigned* out){
    if(out)*out=0;const unsigned count=box==64?128:(box==192||box==224)?32:0;
    if(!count||slot<1||slot>=capacity()+2||index>=count||player>=3)return false;
    for(unsigned i=1;i<12;++i)if(id(slots()[i])>=156)return false;
    if(id(items[box+index])>=156)return false;
    const auto saved=items;original::system={};original::system.gm_mode=game_mode;original::system.gm_flg=side_pack?0x8000000:0;original::system.ply_id=player;original::system.stg_no=stage;original::system.rom_no=room;original::message=0;original::swork={};original::swork.pip=slots();original::swork.bxp=items.data()+box;original::swork.listcsr_0=slot;original::swork.listcsr_2=slot;original::sakai=2;while(original::sakai<12&&(itemdata[id(slots()[original::sakai])].type&0x100))++original::sakai;
    original::ItemBoxChange(&original::swork,std::uint16_t(index));if(out)*out=original::message;
    if(original::message||items==saved||occupied()>capacity()){items=saved;return false;}compact();return true;
}

}
