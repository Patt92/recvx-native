#include "object_itmbox.h"
#include "world.h"
#include "asset_io.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    _Make_SinTable();
    unsigned control=0x80000001u,status=0x40018020u;
    std::vector<unsigned> sounds{7};ItemBoxObjectWork lid{100,0,999,1234};
    for(int pass=1;pass<=41;++pass){
        control_item_box_object(lid,control,status,sounds);
        check(lid.ct0==pass*384,"opening counter does not advance exactly once");
        check(lid.mode0==unsigned(pass==41?2:1),"opening threshold is not strictly greater than 15360");
        check(control==0x80000001u&&status==0x40018020u,"opening changes unrelated flags");
        check(sounds==std::vector<unsigned>({7,0x80000240u}),"opening sound missing, repeated, or preceding sounds overwritten");
        if(pass==1)check(lid.ax==-536,"first opening sample differs");
        if(pass==2)check(lid.ax==-1071,"second opening sample differs");
        if(pass==20)check(lid.ax==-9780,"middle opening sample differs");
        if(pass==40)check(lid.ax==-14493,"strict threshold sample differs");
        if(pass==41)check(lid.ax==-14536,"final opening sample differs");
    }
    control_item_box_object(lid,control,status,sounds);
    check(lid.mode0==3&&lid.ct0==15744&&lid.ax==-14536&&control==0x80040001u,"menu request occurs on wrong pass");
    control_item_box_object(lid,control,status,sounds);
    check(lid.type==0&&lid.ax==0&&lid.mode0==3&&status==0x40008020u&&control==0x80040001u,"return clears more than the original player bit");
    lid={100,17,-6,89};const auto saved=sounds;
    control_item_box_object(lid,control,status,sounds);
    check(lid.mode0==17&&lid.ct0==-6&&lid.ax==89&&sounds==saved,"unknown mode invented an action");
    check(argc==2,"original data directory required");
    const auto data=std::filesystem::path(argv[1]);unsigned cases=0;
    for(const char* name:{"RM_1080","RM_3110","RM_4040","RM_6040","RM_9040"}){
        const auto bytes=read_asset_file(data/"rooms"/(std::string(name)+".RDT"),64*1024*1024);
        const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},name);
        Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
        world.bind_player_collision(events);events.flags[12][0]|=0x200;
        const auto area=std::find_if(events.interactions.begin(),events.interactions.end(),[](const auto& a){return (a.flags&1)&&a.type==4&&(a.attribute&2)&&a.parameters[0]!=255;});
        check(area!=events.interactions.end(),"original lid interaction missing");
        const unsigned object=area->parameters[0];
        const auto* work=world.state(2,object);check(work&&work->object_flags&1,"original lid work missing");
        // Real EventVm interaction with original ATR midpoint; no type-100 injection.
        actor.position={area->position.x+area->extent.x*.5f,area->position.y,area->position.z+area->extent.z*.5f+2};
        actor.yaw=0;events.item_box_object=[&](unsigned index){return world.start_item_box(index);};events.set_player_mode=[&](unsigned m){world.set_player_mode(m);};world.set_player_mode(3u<<16);
        check(events.interact(actor.position,actor.yaw,area->floor),"original box interaction did not start");
        check(work->type==100&&(events.flags[14][0]&0x18000)==0x18000&&(events.flags[5][0]&4),"box start lacks original object/player flags");
        check((events.flags[13][0]&0x10000)&&events.player_flags==events.flags[13][0],"box interaction omits original player work flag");
        check(world.player_mode()==1u,"box interaction omits *(int*)&pp->mode0 = 1 (hitchk.c:4918-4921)");
        world.set_simulation_flags(~4u);world.control_item_boxes(events.flags[10][0],events.flags[14][0]);
        check(work->angles[0]==0&&!(events.flags[10][0]&0x40000),"paused object task still advances lid");
        world.set_simulation_flags(~0u);Scene closed=scene;world.append(closed);
        for(unsigned pass=1;pass<=41;++pass){world.control_item_boxes(events.flags[10][0],events.flags[14][0]);check(!(events.flags[10][0]&0x40000),"World requests menu too early");}
        check(work->angles[0]==-14536&&work->type==100,"World loses actual lid angle");
        Scene opened=scene;world.append(opened);bool moved=false;
        check(opened.triangles.size()==closed.triangles.size(),"lid animation loses geometry");
        for(unsigned t=0;t<opened.triangles.size();++t)for(unsigned v=0;v<3;++v){const auto a=opened.triangles[t].vertices[v].position,b=closed.triangles[t].vertices[v].position;moved=moved||std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.001f;}
        check(moved,"lid angle does not reach rendered geometry");
        world.control_item_boxes(events.flags[10][0],events.flags[14][0]);check(events.flags[10][0]&0x40000,"World never requests item-box menu");
        world.control_item_boxes(events.flags[10][0],events.flags[14][0]);check(work->type==0&&work->angles[0]==0&&!(events.flags[14][0]&0x10000),"World return leaves lid/player bit active");
        world.control_item_boxes(events.flags[10][0],events.flags[14][0]);check(work->type==0&&work->angles[0]==0,"released lid is still controlled");
        ++cases;
    }
    {
        // RM_0160 ATR 4: type 4 ohne attr 0x2, prm0 22 >= rom->itm_n 3. hitchk.c:4863 liest itwp[22]; der
        // Pool ist seit bhInitObjItm (objitm.c:147) genullt, flg 0x1 fehlt, der Fall geht in den
        // Objektzweig (:4903) und startet die Kiste obwp[22]. Mit dem Weltbesitzer wie in runtime.cpp.
        const auto bytes=read_asset_file(data/"rooms"/"RM_0160.RDT",64*1024*1024);
        const auto scene=decode_scene(bytes);auto actor=scene.spawns.at(0);
        std::size_t motion=42;std::int64_t frame=0;AudioMixer audio(data);
        RoomWorld world(bytes,scene,actor,motion,frame,audio,data,{},"RM_0160");
        Inventory inventory;MessageDialog dialog;EventVm events(bytes,inventory,dialog);
        world.bind_player_collision(events);events.flags[12][0]|=0x200;
        events.item_work=[&](unsigned index,unsigned& flg,unsigned& id){const auto* e=world.state(3,index);if(!e)return false;flg=e->object_flags;id=e->identity;return true;};
        check(events.interactions.size()>4,"RM_0160 ATR 4 missing");
        const auto& area=events.interactions[4];
        check((area.flags&1)&&area.type==4&&!(area.attribute&2)&&area.parameters[0]==22&&!world.state(3,22),"RM_0160 ATR 4 is not the itwp fall-through entry");
        const auto* work=world.state(2,22);check(work&&work->identity==376&&(work->object_flags&1),"RM_0160 box work obwp[22] missing");
        actor.position={area.position.x+area.extent.x*.5f,area.position.y,area.position.z+area.extent.z*.5f+2};
        actor.yaw=0;events.item_box_object=[&](unsigned index){return world.start_item_box(index);};events.set_player_mode=[&](unsigned m){world.set_player_mode(m);};world.set_player_mode(3u<<16);
        check(events.interact(actor.position,actor.yaw,area.floor),"RM_0160 box interaction did not start");
        check(work->type==100&&(events.flags[14][0]&0x18000)==0x18000&&(events.flags[5][0]&4),"RM_0160 box start lacks original object/player flags");
        check(events.errors.empty(),"RM_0160 box interaction reported a gap");
        ++cases;
    }
    std::cout<<"Native lid timing, sound/flags, pause and drawn geometry in "<<cases<<" original rooms verified.\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
