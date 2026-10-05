#include "doors.h"
#include "asset_io.h"
#include "binary_view.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace recvx;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try{_Make_SinTable();
    check(DoorRequest{0,0,1,0,0,0}.room_name()=="RM_0010.RDX","door room identity");
    check(DoorRequest{0,2,0x1b,0,0,1}.room_name()=="RM_21B1.RDX","hex room identity");
    std::array<unsigned char,52> fade{};auto integer=[&](unsigned offset,int value){std::memcpy(fade.data()+offset,&value,4);};auto real=[&](unsigned offset,float value){std::memcpy(fade.data()+offset,&value,4);};
    real(8,1);real(12,-.1f);real(16,0);integer(20,1);real(32,0);real(36,.1f);real(40,1);integer(44,-1);integer(24,10);
    RECVX_DOOR_STATE state{};state.status=0x1000001f;state.fpP=fade.data();check(recvx_door_configure(&state,1,0,0),"configure fade");
    unsigned ticks=0;while(!(state.status&0x40)&&ticks++<100)recvx_door_step(&state);check((state.status&0x40)&&ticks>20,"original fade completion");check(!recvx_door_configure(&state,2,0,0),"invalid procedure accepted");
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);unsigned cases=0;SoftwareRenderer renderer(320,240);
        for(unsigned type=0;type<100;++type){const auto file=root/"archives/MRY"/(asset_number(type+71)+".bin");if(!std::filesystem::is_regular_file(file))continue;const auto bytes=read_asset_file(file,8*1024*1024);if(bytes.size()<32||std::memcmp(bytes.data(),"MPAC",4))continue;BinaryView view(bytes);std::size_t p=4+view.u32(4),parameter=0;bool model=false;for(unsigned i=0;i<4;++i){const auto size=view.u32(p);if(i==0)model=bytes.at(p+4)=='M'&&bytes.at(p+5)=='D'&&bytes.at(p+6)=='L';if(i==3)parameter=p+4;p+=4+size;}if(!model)continue;
            for(unsigned c=0;c<view.u8(parameter+3);++c){DoorSequence door;door.begin(root,{},DoorRequest{c<<7,0,1,0,type,0});std::size_t visible=0;bool moving=false;std::array<int,3> previous{};
                for(unsigned tick=0;door.active()&&tick<3601;++tick){door.tick();const auto& state=door.state();moving|=tick>0&&state.objP[1].ang[1]!=previous[1];previous={state.objP[1].ang[0],state.objP[1].ang[1],state.objP[1].ang[2]};if(tick%15==0){door.draw(renderer);visible=std::max(visible,renderer.covered_pixels());}}
                check(door.finished()&&!door.skipped(),"original door does not finish");check(visible>50,"original door not rendered");if(type==0)check(moving,"cell door hinge remains static");++cases;
                door.begin(root,{},DoorRequest{c<<7,0,1,0,type,0});door.tick();door.skip();check(door.finished()&&door.skipped()&&!door.active(),"door skip state");
            }
        }check(cases>=6,"original door assets missing");std::cout<<cases<<" original door cases rendered, finished and skipped.\n";
    }
    std::cout<<"Door destination and original fade tests passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
