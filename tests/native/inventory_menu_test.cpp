#include "ui.h"
#include "events.h"
#include "item_asset.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static Inventory empty(){Inventory inv;std::fill(inv.slots()+2,inv.slots()+12,0);return inv;}
static Message fixture_message(unsigned index){
    if(index>=181&&index<=187)return {'L'-32,0xff02,0,'?'-32,0xff04,'Y'-32,0xff04,'N'-32,0xffff};
    return {'T'-32,0xffff};
}
static void select_action(StatusMenu& menu,Inventory& inv,unsigned slot,unsigned action){
    if(!menu.active)menu.input(MenuInput::Open,inv);menu.selected=slot;menu.input(MenuInput::Confirm,inv);
    for(unsigned i=0;i<action;++i)menu.input(MenuInput::Down,inv);menu.input(MenuInput::Confirm,inv);
}
static void acknowledge(StatusMenu& menu,Inventory& inv){for(unsigned i=0;i<20&&menu.message.active();++i)menu.input(MenuInput::Confirm,inv);check(!menu.message.active(),"message cannot be dismissed");}
static std::vector<std::uint8_t> use_fixture(){
    std::vector<std::uint8_t> bytes(1500);auto put=[&](unsigned p,unsigned n){for(unsigned i=0;i<4;++i)bytes.at(p+i)=std::uint8_t(n>>(8*i));};
    auto real=[&](unsigned p,float n){std::memcpy(bytes.data()+p,&n,4);};real(0,1.73f);put(16,64);put(28,1100);put(64+0x38,1400);
    put(64+0x20,300);put(64+0xa0,1);bytes[300]=1;real(320,20);real(324,20);real(328,20);bytes[332]=50;
    put(1100,16);put(1104,18);put(1108,80);put(1112,110);
    const std::vector<std::uint8_t> use={1,12,0x10,50,0x31,50,5,1,0,0,0,0,3,0,0xff,0};std::copy(use.begin(),use.end(),bytes.begin()+1180);
    const std::vector<std::uint8_t> change={0x7c,0,0,0,85,0,0,0,0,0,0xff,0};std::copy(change.begin(),change.end(),bytes.begin()+1210);return bytes;
}
int main(int argc,char** argv){try{
    _Make_SinTable();std::unique_ptr<GameUi> ui;if(argc>1)ui=std::make_unique<GameUi>(std::filesystem::u8path(argv[1]));
    if(argc>2)std::filesystem::create_directories(std::filesystem::u8path(argv[2]));
    auto messages=[&](StatusMenu& menu){menu.message_source=[&](unsigned index){return ui?ui->system_message(index):fixture_message(index);};};
    const unsigned recipes[][3]={{21,21,24},{21,22,25},{21,23,26},{21,24,28},{21,26,27},{22,26,29},{23,24,27},{23,25,29}};
    for(const auto& recipe:recipes)for(unsigned reverse=0;reverse<2;++reverse){
        auto inv=empty();inv.slots()[2]=Inventory::pack(recipe[reverse?1:0],1);inv.slots()[3]=Inventory::pack(recipe[reverse?0:1],1);StatusMenu menu;messages(menu);
        select_action(menu,inv,2,2);check(menu.combining&&menu.combine_from==2,"Combine does not retain its source");menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);
        check(!menu.combining&&menu.active&&inv.occupied()==1&&Inventory::id(inv.slots()[menu.selected])==recipe[2],"herb recipe/result cursor failed through menu");
    }
    {auto inv=empty();inv.slots()[2]=Inventory::pack(22,1);inv.slots()[3]=Inventory::pack(23,1);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);const auto saved=inv.items;menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);
        check(inv.items==saved&&menu.combining&&menu.feedback_sound==1,"invalid herbs consumed or combination exited");menu.input(MenuInput::Cancel,inv);check(!menu.combining&&menu.selected==2,"combine cancel loses source cursor");
        select_action(menu,inv,2,0);check(menu.message.index()==161&&menu.active&&inv.items==saved,"red herb use closes inventory or consumes it");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(21,1);inv.slots()[3]=Inventory::pack(22,1);for(unsigned i=4;i<10;++i)inv.slots()[i]=Inventory::pack(95,1);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);menu.input(MenuInput::Right,inv);menu.input(MenuInput::Confirm,inv);check(inv.contains(25)&&inv.occupied()==7,"full inventory prevents an in-place herb combination");inv.hp=10;inv.poisoned=true;select_action(menu,inv,menu.selected,0);check(inv.hp==160&&inv.poisoned&&!inv.contains(25)&&menu.active,"mixed herb use fails or incorrectly cures poison");}
    for(const auto counts:{std::pair<unsigned,unsigned>{4,10},{10,4},{4,4}})for(unsigned reverse=0;reverse<2;++reverse){auto inv=empty();inv.slots()[2]=Inventory::pack(35,counts.first);inv.slots()[3]=Inventory::pack(18,counts.second);unsigned result=0;check(inv.combine(reverse?3:2,reverse?2:3,nullptr,&result),"partial bowgun powder conversion rolled back");check(Inventory::id(inv.slots()[result])==36&&Inventory::quantity(inv.slots()[result])==std::min(counts.first,counts.second),"conversion loses explosive arrow count/result cursor");check(inv.contains(18)==(counts.second>counts.first)&&inv.contains(35)==(counts.first>counts.second),"conversion loses leftover arrows/powder");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(5,18);inv.slots()[3]=Inventory::pack(12,30);inv.equip(2);StatusMenu menu;messages(menu);select_action(menu,inv,2,2);menu.input(MenuInput::Right,inv);const auto saved=inv.items;menu.input(MenuInput::Confirm,inv);check(inv.items==saved&&menu.message.index()==156&&menu.combining,"full magazine mutates inventory or loses original dialogue");acknowledge(menu,inv);menu.input(MenuInput::Cancel,inv);check(inv.weapon()==3,"failed reload unequips handgun");}
    for(unsigned item:{63u,128u,129u,83u}){
        auto inv=empty();inv.slots()[2]=Inventory::pack(item,1);for(unsigned i=3;i<10;++i)inv.slots()[i]=Inventory::pack(95,1);StatusMenu menu;messages(menu);select_action(menu,inv,2,1);check(menu.examining,"Examine action missing");
        if(item!=83){menu.input(MenuInput::Confirm,inv);check(menu.message.index()==188&&inv.contains(item),"locked case opens without lockpick");acknowledge(menu,inv);inv.slots()[3]=Inventory::pack(50,1);}
        menu.examine_yaw=32768;menu.input(MenuInput::Confirm,inv);check(menu.message.index()==item,"back of case activates its lock/button");acknowledge(menu,inv);menu.examine_yaw=0;menu.input(MenuInput::Confirm,inv);
        check(menu.message.index()==(item==83?182u:181u),"original opening question missing");while(menu.message.active()&&!menu.message.choices())menu.input(MenuInput::Confirm,inv);
        menu.input(MenuInput::Right,inv);const auto original=inv.items;menu.input(MenuInput::Confirm,inv);check(!menu.opening_item&&inv.items==original,"No opens/consumes case");
        menu.input(MenuInput::Confirm,inv);menu.input(MenuInput::Cancel,inv);check(!menu.message.active()&&inv.items==original,"cancel opens/consumes case");
        menu.input(MenuInput::Confirm,inv);while(menu.message.active()&&!menu.message.choices())menu.input(MenuInput::Confirm,inv);menu.input(MenuInput::Confirm,inv);check(menu.opening_item==item,"Yes does not start case action");
        if(ui&&argc>2){SoftwareRenderer image(640,480);MessageDialog room;ui->draw(image,inv,menu,room,0);image.save_bmp(std::filesystem::u8path(argv[2])/(asset_number(item)+"-closed.bmp"));}
        for(unsigned frame=0;frame<22;++frame)menu.tick(inv);check(inv.items==original&&menu.opening_frame==22,"case changes inventory before opening animation ends");
        if(ui){const auto packet=read_asset_file(std::filesystem::u8path(argv[1])/"archives/ITEM1"/(asset_number(item-1)+".bin"),8*1024*1024);const auto closed=decode_item_asset(packet,0,item),opened=decode_item_asset(packet,22,item);check(closed.triangles.size()==opened.triangles.size(),"case action loses model parts");bool moved=false;for(unsigned i=0;i<closed.triangles.size();++i)for(unsigned j=0;j<3;++j){const auto a=closed.triangles[i].vertices[j].position,b=opened.triangles[i].vertices[j].position;moved|=std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.001f;}check(moved,"original case lid/catch animation is static");SoftwareRenderer image(640,480);MessageDialog room;ui->draw(image,inv,menu,room,22);if(argc>2)image.save_bmp(std::filesystem::u8path(argv[2])/(asset_number(item)+"-opening.bmp"));}
        menu.tick(inv);const auto expected=item==63?19u:item==128?35u:item==129?30u:85u;check(!menu.opening_item&&!inv.contains(item)&&inv.contains(expected)&&inv.occupied()==8,("case contents missing/full inventory: item="+std::to_string(item)+" expected="+std::to_string(expected)+" actual="+std::to_string(Inventory::id(inv.slots()[2]))+" occupied="+std::to_string(inv.occupied())).c_str());check(item==83||inv.contains(50),"opening consumes lockpick");check(menu.message.index()==(item==63?162u:item==128?164u:item==129?163u:165u),"original contents dialogue missing");if(ui&&argc>2){SoftwareRenderer image(640,480);MessageDialog room;ui->draw(image,inv,menu,room,23);image.save_bmp(std::filesystem::u8path(argv[2])/(asset_number(item)+"-contents.bmp"));}acknowledge(menu,inv);menu.input(MenuInput::Cancel,inv);check(!menu.examining&&menu.active,"contents view cannot return to inventory");if(item==129)check(inv.contains(13)&&!inv.contains(30),"case magnum rounds do not normalize on inspection exit");
    }
    {auto inv=empty();inv.slots()[2]=Inventory::pack(50,1);inv.slots()[3]=Inventory::pack(59,1);inv.slots()[4]=Inventory::pack(85,1);inv.slots()[5]=Inventory::pack(12,30);MessageDialog room;auto data=use_fixture();EventVm events(data,inv,room);unsigned index=0;
        check(!events.use(50,&index)&&index==161&&!(events.flags[10][0]&0x400),"key outside floor starts an event");events.check_floor({5,0,5},0);check(events.use(50,&index)&&events.values[8]==50,"original floor item parameters are not accepted");events.start(0,0);events.tick();check(!inv.contains(50)&&events.flag(1,0)&&events.errors.empty(),"original UseItemCheck/consume/flag script does not commit");
        events.start(0,1);events.tick();events.check_floor({5,0,5},0);check(!events.use(59,&index)&&index==176,"updated TG-01 trigger loses original unprocessed-material dialogue");check(!events.use(12,&index)&&index==174,"machine rejects ammo without original size dialogue");check(events.use(85,&index),"bhFlrAtariSet2 does not update allowed key");events.check_floor({100,0,100},0);check(!events.use(12,&index)&&index==160,"ammo outside trigger has no original rejection dialogue");}
    {auto inv=empty();inv.slots()[2]=Inventory::pack(82,1);MessageDialog room;auto data=use_fixture();data[332]=82;EventVm events(data,inv,room);events.check_floor({5,0,5},0);unsigned index=0;check(events.use(82,&index)&&inv.contains(82)&&!Inventory::quantity(inv.slots()[2]),"extinguisher use deletes it or fails to empty it");events.values[0]=9;events.values[1]=8;check(!events.use(82,&index)&&index==195,"empty extinguisher has no original Antarctica dialogue");}
    std::cout<<"All herb recipes in both orders, menu failures/cancel/full capacity, partial powder conversion and four original case/briefcase outcomes passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
