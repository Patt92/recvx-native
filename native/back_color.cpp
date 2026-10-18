#include "back_color.h"
#include <stdexcept>
namespace recvx {
namespace {
// Pure data from ps2_event.c:14-155 (room) and :255-324 (event).
// Preserve order: stage 6 room 6 cut 2 reads the following row, not a second match.
constexpr BackColorRow room0[]={
    {12,0,1,0,0,0x1e1a10u,0x000000u},
    {12,0,2,0,0,0x1e1a10u,0x000000u},
    {12,0,3,0,0,0x1e1a10u,0x000000u},
    {12,0,4,0,0,0x1e1a10u,0x000000u},
};
constexpr BackColorRow room1[]={
    {7,0,0,0,0,0x202015u,0x000000u},
    {7,0,1,0,0,0x202018u,0x000000u},
    {11,0,2,0,0,0x1a1910u,0x000000u},
};
constexpr BackColorRow room2[]={
    {4,0,0,0,0,0x201921u,0x000000u},
    {4,0,1,0,0,0x201921u,0x000000u},
    {6,0,0,0,0,0x201321u,0x000000u},
};
constexpr BackColorRow room3[]={
    {3,0,0,0,0,0x031010u,0x000000u},
    {4,0,1,0,0,0x181310u,0x000000u},
    {6,0,0,0,0,0x303030u,0x000000u},
    {6,0,1,0,0,0x282828u,0x000000u},
    {6,0,3,0,0,0x202020u,0x000000u},
    {6,0,4,0,0,0x505030u,0x000000u},
    {8,0,1,0,0,0x202028u,0x000000u},
    {8,0,3,0,0,0x141621u,0x000000u},
    {10,0,6,0,0,0x18181cu,0x000000u},
    {10,0,8,0,0,0x101219u,0x000000u},
    {15,0,0,0,0,0x202018u,0x000000u},
    {15,0,1,0,0,0x181818u,0x000000u},
    {16,0,0,0,0,0x0c0c1cu,0x000000u},
    {16,0,1,0,0,0x0c0c1cu,0x000000u},
    {21,0,0,0,0,0x101318u,0x000000u},
};
constexpr BackColorRow room4[]={
    {0,0,0,0,0,0x101e20u,0x000000u},
    {0,0,1,0,0,0x101e20u,0x000000u},
    {0,0,2,0,0,0x101e20u,0x000000u},
};
constexpr BackColorRow room5[]={
    {60,0,0,0,0,0x10363bu,0x000000u},
    {60,0,2,0,0,0x10363bu,0x000000u},
    {60,0,5,0,0,0x10363bu,0x000000u},
    {60,0,6,0,0,0x10363bu,0x000000u},
    {60,0,8,0,0,0x10363bu,0x000000u},
    {60,0,9,0,0,0x10363bu,0x000000u},
    {64,0,1,0,0,0x201f20u,0x000000u},
    {66,0,0,0,0,0x202018u,0x000000u},
    {66,0,1,0,0,0x181818u,0x000000u},
    {68,0,0,0,0,0xb0c0d0u,0x000000u},
    {68,0,1,0,0,0x90a0b0u,0x000000u},
    {68,0,2,0,0,0x90a0b0u,0x000000u},
    {68,0,3,0,0,0x606870u,0x000000u},
    {69,0,0,0,0,0x15110au,0x000000u},
    {71,0,0,0,0,0x101e20u,0x000000u},
    {71,0,1,0,0,0x101e20u,0x000000u},
    {71,0,2,0,0,0x101e20u,0x000000u},
};
constexpr BackColorRow room6[]={
    {1,0,0,0,0,0x404040u,0x000000u},
    {1,0,1,0,0,0x404040u,0x000000u},
    {1,0,2,0,0,0x404040u,0x000000u},
    {1,0,4,0,0,0x404040u,0x000000u},
    {1,0,6,0,0,0x404040u,0x000000u},
    {3,0,0,0,1,0x10272cu,0x21343au},
    {3,0,1,0,1,0x1b3c43u,0x21343au},
    {3,0,2,0,1,0x1b3c43u,0x21343au},
    {3,0,4,0,1,0x10272cu,0x1a2324u},
    {5,0,0,0,1,0x12181fu,0x10363bu},
    {5,0,1,0,1,0x12181fu,0x10363bu},
    {5,1,1,0,1,0x12181fu,0x10363bu},
    {5,0,2,0,1,0x12181fu,0x10363bu},
    {5,0,3,0,0,0x12120fu,0x000000u},
    {5,0,5,0,1,0x12181fu,0x10363bu},
    {5,0,6,0,1,0x12181fu,0x10363bu},
    {5,0,8,0,1,0x12181fu,0x10363bu},
    {5,0,9,0,1,0x12181fu,0x10363bu},
    {6,0,0,0,1,0x25404bu,0x50807au},
    {6,0,1,0,1,0x1d343cu,0x49716eu},
    {6,0,2,0,3,0x304d50u,0x000000u},
    {6,0,2,0,3,0x5a8781u,0x889850u},
    {6,0,3,0,1,0x304d50u,0x5a8781u},
};
constexpr BackColorRow room7[]={
    {5,0,1,0,0,0x00110eu,0x000000u},
    {7,0,0,0,0,0x205070u,0x000000u},
    {7,0,2,0,0,0x205070u,0x000000u},
    {7,0,3,0,0,0x103c50u,0x000000u},
    {7,1,0,0,0,0x205070u,0x000000u},
    {7,1,2,0,0,0x205070u,0x000000u},
    {7,1,3,0,0,0x103c50u,0x000000u},
    {11,0,2,0,0,0x181818u,0x000000u},
    {11,0,4,0,0,0x181818u,0x000000u},
    {15,0,4,0,0,0x1d1d00u,0x000000u},
};
constexpr BackColorRow room8[]={
    {0,0,0,0,0,0x101e20u,0x000000u},
    {0,0,1,0,0,0x101e20u,0x000000u},
    {0,0,2,0,0,0x101e20u,0x000000u},
};
constexpr BackColorRow room9[]={
    {1,0,0,0,0,0x404040u,0x000000u},
    {1,0,1,0,0,0x404040u,0x000000u},
    {1,0,2,0,0,0x404040u,0x000000u},
    {1,0,4,0,0,0x404040u,0x000000u},
    {1,0,6,0,0,0x404040u,0x000000u},
    {3,0,0,0,0,0x21343au,0x000000u},
    {3,0,1,0,0,0x21343au,0x000000u},
    {3,0,2,0,0,0x21343au,0x000000u},
    {3,0,4,0,0,0x1a2324u,0x000000u},
    {9,0,0,0,0,0x23292cu,0x000000u},
    {10,0,0,0,0,0x293735u,0x000000u},
    {10,0,1,0,0,0x162a23u,0x000000u},
    {10,0,3,0,0,0x0e1a18u,0x000000u},
    {12,0,0,0,2,0x102025u,0xb0c0d0u},
    {12,0,2,0,2,0x000060u,0xe1f2feu},
    {12,0,3,0,2,0x222729u,0x606870u},
    {17,0,1,0,0,0x082424u,0x000000u},
    {17,0,3,0,0,0x082424u,0x000000u},
    {17,0,4,0,0,0x082424u,0x000000u},
    {20,0,0,0,0,0x242424u,0x000000u},
    {27,0,1,0,0,0x201f20u,0x000000u},
};
constexpr const BackColorRow* room_rows[]={room0,room1,room2,room3,room4,room5,room6,room7,room8,room9};
constexpr unsigned room_counts[]={4,3,3,15,3,17,23,10,3,21};
constexpr BackColorRow event3[]={
    {23,0,6,0,0,0x386880u,0x000000u},
    {23,0,10,0,0,0x386880u,0x000000u},
    {23,0,11,0,0,0x386880u,0x000000u},
};
constexpr BackColorRow event6[]={
    {3,0,0,0,0,0x10272cu,0x000000u},
};
constexpr BackColorRow event7[]={
    {7,0,0,0,0,0x506070u,0x000000u},
    {7,0,2,0,0,0x304060u,0x000000u},
    {7,0,10,0,0,0x304860u,0x000000u},
    {7,0,11,0,0,0x506070u,0x000000u},
    {7,0,14,0,0,0x505860u,0x000000u},
    {7,0,15,0,0,0x305050u,0x000000u},
    {7,0,19,0,0,0x305060u,0x000000u},
    {7,0,20,0,0,0x608090u,0x000000u},
    {7,0,24,0,0,0x274359u,0x000000u},
    {7,0,25,0,0,0x304040u,0x000000u},
    {7,0,26,0,0,0x306080u,0x000000u},
    {7,0,28,0,0,0x306078u,0x000000u},
    {7,0,30,0,0,0x304040u,0x000000u},
};
constexpr BackColorRow event9[]={
    {9,0,44,0,0,0x14171au,0x000000u},
    {9,0,58,0,0,0x14171au,0x000000u},
    {28,0,10,0,0,0x0a121bu,0x000000u},
    {28,0,34,0,0,0x0a121bu,0x000000u},
    {35,0,1,0,0,0x909870u,0x000000u},
    {35,0,6,0,0,0x909870u,0x000000u},
    {35,0,11,0,0,0x5b6c8au,0x000000u},
    {35,0,18,0,0,0x181c20u,0x000000u},
    {35,0,32,0,0,0x181c20u,0x000000u},
    {35,0,37,0,0,0x181c20u,0x000000u},
    {35,0,38,0,0,0x404040u,0x000000u},
    {35,0,44,0,0,0x181c20u,0x000000u},
    {35,0,45,0,0,0x909870u,0x000000u},
};
constexpr const BackColorRow* event_rows[]={nullptr,nullptr,nullptr,event3,nullptr,nullptr,event6,event7,nullptr,event9};
constexpr unsigned event_counts[]={0,0,0,3,0,0,1,13,0,13};
}
std::uint32_t back_color_row(const BackColorRow& row,const BackColorRow* following,
                           std::uint32_t fog,std::uint32_t event9,std::uint32_t event12,bool event_camera){
    if(row.flag)return fog;
    switch(row.event){
    case 0:return row.color;
    case 1:return (event9&0x400000u)?row.event_color:row.color;
    case 2:return (event12&0x20u)?row.event_color:row.color;
    case 3:
        if(event_camera)throw std::runtime_error("bhChangeBackColorEvt has no ev_flag 3 branch");
        if(!(event9&0x400000u))return row.color;
        if(!following)throw std::runtime_error("bhChangeBackColor ev_flag 3 has no following row");
        return (event9&0x8000000u)&&!(event9&0x10000000u)?following->event_color:following->color;
    default:throw std::runtime_error("unbound background event selector");
    }
}
namespace {std::uint32_t installed_clear=0;} // main.c:176 njSetBackColor(0,0,0) before the first task
void set_back_color(std::uint32_t argb){installed_clear=argb;}
std::uint32_t installed_back_color(){return installed_clear;}
std::uint32_t camera_back_color(unsigned stage,unsigned room,unsigned rcase,unsigned camera,bool event_camera,
                                std::uint32_t room_color,std::uint32_t fog,std::uint32_t event9,std::uint32_t event12){
    if(stage>=10)throw std::runtime_error("background stage outside original table");
    const auto rows=event_camera?event_rows[stage]:room_rows[stage];
    const auto count=event_camera?event_counts[stage]:room_counts[stage];
    for(unsigned i=0;i<count;++i){const auto& row=rows[i];
        if(unsigned(row.room)==room&&unsigned(row.rcase)==rcase&&unsigned(row.camera)==camera)
            return back_color_row(row,i+1<count?&rows[i+1]:nullptr,fog,event9,event12,event_camera);
    }
    return room_color;
}
}
