#include "doors.h"
#include "asset_io.h"
#include "binary_view.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace recvx;
// PORTIERUNGSLÜCKE (Policy 2026-10-09): Dieser Test prüfte die Türdemo aus
// door_procedures.inc (FadeProc1-Verlauf, Kamerafahrt, Türbewegung, die
// Sound-Anforderungsgruppen und das gerenderte Türbild). Generator und Include
// sind Quarantäne; die Prozeduren und die Parametergrößen fehlen, siehe
// native/door_core.c. Geprüft bleiben der Raumname aus den Türdaten, der
// unveränderte MPAC-Datenpfad von DoorSequence und dass die Lücke sich wie
// dokumentiert verhält: keine gebundene Prozedur, keine Parametergröße, und
// ein Schritt meldet sofort den Abschlussstatus, statt hängen zu bleiben.
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try{_Make_SinTable();
    check(DoorRequest{0,0,1,0,0,0}.room_name()=="RM_0010.RDX","door room identity");
    check(DoorRequest{0,0,16,0,0,0}.room_name()=="RM_0160.RDX","two-digit decimal destination");
    check(DoorRequest{0,2,0x1b,0,0,1}.room_name()=="RM_2271.RDX","original decimal room identity");
    for(unsigned family=0;family<3;++family)for(unsigned type=0;type<8;++type)
        check(recvx_door_parameter_size(family,type)==0,"door parameter size gap reports a guessed block size");
    for(unsigned type=0;type<8;++type)check(recvx_door_validate_view(type,nullptr,0,0),"door view type range rejected");
    check(!recvx_door_validate_view(8,nullptr,0,0),"door view type outside 0..7 accepted");
    RECVX_DOOR_STATE state{};state.status=0x1000001f;
    check(!recvx_door_configure(&state,1,0,0),"door configure claims a bound procedure");
    check(!state.fde_prcP&&!state.vew_prcP&&!state.dor_prcP,"door configure installed a procedure");
    recvx_door_step(&state);check(state.status&0x40,"door gap does not report the documented completion status");
    if(argc>1){const auto root=std::filesystem::u8path(argv[1]);unsigned cases=0;
        for(unsigned type=0;type<100;++type){const auto file=root/"archives/MRY"/(asset_number(type+71)+".bin");if(!std::filesystem::is_regular_file(file))continue;
            const auto bytes=read_asset_file(file,8*1024*1024);if(bytes.size()<32||std::memcmp(bytes.data(),"MPAC",4))continue;
            BinaryView view(bytes);std::size_t p=4+view.u32(4),parameter=0;bool model=false;
            for(unsigned i=0;i<4;++i){const auto size=view.u32(p);if(i==0)model=bytes.at(p+4)=='M'&&bytes.at(p+5)=='D'&&bytes.at(p+6)=='L';if(i==3)parameter=p+4;p+=4+size;}
            if(!model)continue;
            for(unsigned c=0;c<view.u8(parameter+3);++c){
                DoorSequence door;door.begin(root,{},DoorRequest{c<<7,0,1,0,type,0});
                check(door.active(),"original door packet no longer starts a sequence");
                door.tick();
                check(door.finished()&&!door.active()&&!door.skipped(),"door gap does not finish in its first step");
                check(door.take_sounds().empty(),"door gap requested a sound without a procedure");
                ++cases;
            }
        }
        check(cases>=6,"original door assets missing");
        std::cout<<cases<<" original door packets still parse; their demo is a documented gap.\n";
    }
    std::cout<<"Door destination and the documented door-procedure gap passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
