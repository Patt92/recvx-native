#define SDL_MAIN_HANDLED
#include "world.h"
#include "asset_io.h"
#include "room.h"
#include "audio.h"
#include "ninja_matrix.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace {
// Faehrt einen Raum so weit, dass die Gegnerpasses gelaufen sind.
struct Room {
    std::filesystem::path data;std::vector<std::uint8_t> bytes;Scene scene;SceneSpawn player;
    std::size_t motion=42;std::int64_t frame=0;std::unique_ptr<AudioMixer> audio;std::unique_ptr<RoomWorld> world;
    Room(const std::filesystem::path& root,const std::string& name,unsigned ticks){
        data=root;bytes=read_asset_file(data/("rooms/"+name+".RDT"),64*1024*1024);scene=decode_scene(bytes);player=scene.spawns.at(0);
        audio=std::make_unique<AudioMixer>(data);
        world=std::make_unique<RoomWorld>(bytes,scene,player,motion,frame,*audio,data);
        for(unsigned i=0;i<ticks;++i)world->tick();
    }
    unsigned selected(unsigned identity)const{unsigned n=0;for(unsigned i=0;i<64;++i){const auto* e=world->state(1,i);if(e&&e->identity==identity&&e->morph_obj_a==0&&e->morph_obj_b==1)++n;}return n;}
    bool gapped(const char* routine)const{const auto& g=world->unbound_enemy_routines();return g.find(routine)!=g.end();}
};
}
int main(int argc,char** argv){try{
    if(argc<2)return 0;
    _Make_SinTable();
    const std::filesystem::path data=argv[1];
    // en01.c:2749 bhEne01_Init: der normale Zombie waehlt obj_a = mbp[0],
    // obj_b = mbp[1]. Vor der Verdrahtung lief das in die Luecke
    // "mlwP->objP = mbp[0] (en01.c:2749)".
    {
        Room room(data,"RM_5680",120);
        const auto zombies=room.selected(1);
        std::cout<<"RM_5680 zombies with obj_a/obj_b = mbp[0]/mbp[1]: "<<zombies<<"\n";
        check(!room.gapped("mlwP->objP = mbp[0] (en01.c:2749)"),"en01 select_morph_pair is still unbound");
        check(zombies,"en01 select_morph_pair did not reach the work");
    }
    // en30.c:176 morph_sources und en14.c:457 morph_models waehlen dasselbe Paar.
    // en30 hat seinen Erzeuger im Raum; en14s Morphzweig haengt zusaetzlich am
    // EN13-Elternteil (en14.c:448), der noch nicht portiert ist, also bleibt dort
    // nur der Nachweis, dass der Dienst gebunden ist.
    {
        Room room(data,"RM_9302",180);
        const auto en30=room.selected(30);
        std::cout<<"RM_9302 en30 with obj_a/obj_b = mbp[0]/mbp[1]: "<<en30<<"\n";
        check(!room.gapped("bhEne30_Init: obj_a/obj_b"),"en30 morph_sources is still unbound");
        check(!room.gapped("bhEne14_MV02: Morphmodelle0/1"),"en14 morph_models is still unbound");
        check(en30,"en30 morph_sources did not reach the work");
        check(room.selected(14)==0,"en14 selected a morph pair without its EN13 parent");
    }
    // en12.c:800 morph_sources.
    {
        Room room(data,"RM_9301",180);
        const auto en12=room.selected(12);
        std::cout<<"RM_9301 en12 with obj_a/obj_b = mbp[0]/mbp[1]: "<<en12<<"\n";
        check(!room.gapped("bhEne12_MV02: obj_a/obj_b"),"en12 morph_sources is still unbound");
    }
    std::cout<<"morph world ok\n";return 0;
}catch(const std::exception& error){std::cerr<<"morph world test failed: "<<error.what()<<"\n";return 1;}}
