#include "combat.h"
#include "ui.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    _Make_SinTable();
    for(unsigned owner=0;owner<4;++owner)for(unsigned mode=0;mode<4;++mode)for(unsigned rounds:{0u,1u,18u}){
        Inventory inv;inv.player=owner;inv.game_mode=mode;std::fill(inv.slots(),inv.slots()+12,0);
        inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(5,rounds);inv.slots()[3]=Inventory::pack(95,1);
        const auto before=inv.items;unsigned flags=0;auto q=inv.pickup(12,flags,0,7);
        check(q.mode==1&&inv.items==before,"question changes occupied gun/item slots");
        auto no=inv.pickup(12,flags,0,7,q.mode,101);check(!no.granted&&inv.items==before,"No changes inventory");
        flags=0;q=inv.pickup(12,flags,0,7);auto yes=inv.pickup(12,flags,0,7,q.mode,100);
        check(yes.granted&&inv.slots()[2]==Inventory::pack(5,rounds)&&inv.slots()[3]==Inventory::pack(95,1)&&inv.slots()[4]==Inventory::pack(12,getbulletmax[12][mode])&&inv.slots()[0]==2,"ammo overwrites occupied slot or equipment");
        const auto loaded=inv.items;for(unsigned i=0;i<rounds;++i)check(inv.count_bullet(),"valid round rejected");
        const auto empty=inv.items;for(unsigned i=0;i<1000;++i)check(!inv.count_bullet()&&inv.items==empty,"empty gun underflows or changes ID/flags");
        check(inv.slots()[2]==Inventory::pack(5,0)&&inv.slots()[4]==loaded[owner*16+4],"empty fire consumes reserve ammo");
    }
    for(bool wide:{false,true})for(bool full:{false,true}){
        Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[2]=Inventory::pack(wide?3:5,0);inv.slots()[0]=2;
        if(full)for(unsigned i=3;i<(wide?9u:10u);++i)inv.slots()[i]=Inventory::pack(95,1);
        const auto before=inv.items;unsigned flags=0;auto q=inv.pickup(12,flags,0,7);
        if(full)check(q.mode==3&&!q.granted&&inv.items==before,"full inventory overwrites gun");
        else {auto yes=inv.pickup(12,flags,0,7,q.mode,100);check(yes.granted&&inv.slots()[2]==before[2]&&inv.slots()[3]==Inventory::pack(12,15)&&inv.slots()[0]==2,"wide/empty gun lost during ammo sort");}
    }
    {Inventory inv;inv.slots()[2]=Inventory::pack(5,0);inv.slots()[3]=Inventory::pack(12,990);inv.slots()[0]=2;unsigned flags=0x4000;auto r=inv.pickup(12,flags,0,7);check(r.granted&&inv.slots()[2]==Inventory::pack(5,0)&&inv.slots()[3]==Inventory::pack(12,999)&&inv.slots()[4]==Inventory::pack(12,6),"ammo stack overflow loses gun/rounds");}
    for(unsigned mode=0;mode<4;++mode)for(unsigned gun:{5u,9u,131u})for(bool reserve_first:{false,true})for(unsigned amount:{1u,15u,30u,999u}){
        Inventory inv;inv.game_mode=mode;std::fill(inv.slots(),inv.slots()+12,0);const auto gun_slot=reserve_first?3u:2u,ammo_slot=reserve_first?2u:3u;
        inv.slots()[gun_slot]=Inventory::pack(gun,0);inv.slots()[ammo_slot]=Inventory::pack(12,amount);inv.slots()[0]=gun_slot;
        check(inv.reload(),"original reload rejects pistol reserve");
        const auto loaded=std::min(amount,getbulletmax[gun][mode]);
        check(inv.slots()[inv.slots()[0]]==Inventory::pack(gun,loaded),"original reload loses weapon or uses wrong magazine capacity");
        unsigned sum=loaded;for(unsigned i=2;i<12;++i)if(Inventory::id(inv.slots()[i])==12)sum+=Inventory::quantity(inv.slots()[i]);check(sum==amount,"reload loses ammunition");
    }
    for(unsigned value:{0u,1u,18u,32767u,32768u,65535u})for(unsigned flags:{0u,0x08000000u,0x20000000u}){
        Inventory inv;inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(5,value)|flags;const auto before=inv.slots()[2];const bool infinite=flags&0x08000000;
        check(inv.count_bullet()==(infinite||(value>0&&value<32768)),"signed original bullet count differs");
        check(inv.slots()[2]==(infinite||!value||value>=32768?before:before-1),"bullet decrement corrupts packed item");
    }
    if(argc>1){
        const auto data=std::filesystem::u8path(argv[1]);Inventory inv;std::fill(inv.slots(),inv.slots()+12,0);inv.slots()[0]=2;inv.slots()[2]=Inventory::pack(5,1);
        auto body=PlayerAsset::load(data);AudioMixer audio(data);CombatController combat(data);SceneSpawn actor{{0,0,0},0};std::size_t motion=42;std::int64_t frame=0;
        // The original empty-chamber branch needs a trigger edge (sys->pad_ps 0x100),
        // so repeat the press instead of holding it down.
        for(unsigned i=0;i<300;++i)combat.tick({true,i%2==0},false,inv,body,actor,motion,frame,nullptr,audio);
        check(combat.shots==1&&combat.empty>5&&inv.slots()[2]==Inventory::pack(5,0)&&inv.slots()[0]==2,"repeated empty trigger corrupts pistol");
        GameUi ui(data);StatusMenu menu;menu.input(MenuInput::Open,inv);menu.skip_slide();MessageDialog dialog;SoftwareRenderer image(640,480);
        ui.draw(image,inv,menu,dialog,0);if(argc>2)image.save_bmp(std::filesystem::u8path(argv[2]));
        // With the identical icon and screen, changing only 0 to 1 must change
        // the quantity glyph region.
        const auto zero=image.pixels();inv.slots()[2]=Inventory::pack(5,1);ui.draw(image,inv,menu,dialog,0);
        bool changed=false;for(unsigned y=160;y<176;++y)for(unsigned x=460;x<472;++x)changed|=zero[y*640+x]!=image.pixels()[y*640+x];
        check(changed,"zero and one ammunition glyphs are not rendered distinctly");
        inv.slots()[2]=Inventory::pack(5,0);unsigned flags=0;auto q=inv.pickup(12,flags,0,7);auto yes=inv.pickup(12,flags,0,7,q.mode,100);check(yes.granted&&inv.slots()[3]==Inventory::pack(12,15),"empty pistol pickup lost ammo");
        combat.tick({},false,inv,body,actor,motion,frame,nullptr,audio);
        const auto shots=combat.shots;for(unsigned i=0;i<10;++i)combat.tick({true,false},false,inv,body,actor,motion,frame,nullptr,audio);
        // bhSearchBullet runs from the same trigger edge, so repeat the press.
        for(unsigned i=0;i<120&&combat.shots==shots;++i)combat.tick({true,i%2==0},false,inv,body,actor,motion,frame,nullptr,audio);
        check(combat.shots==shots+1&&inv.slots()[2]==Inventory::pack(5,14)&&!inv.contains(12)&&inv.slots()[0]==2,"automatic reload loses rounds or equipment");
    }
    std::cout<<"Occupied pickup slots (4 owners/4 modes), zero/signed/infinite counts and repeated empty triggers passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
