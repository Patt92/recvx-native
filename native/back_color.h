#pragma once
#include <cstdint>
namespace recvx {
// ps2_event.h EVT_WRK: serialized and host clear colours are 0xAARRGGBB.
struct BackColorRow {int room,rcase,camera,flag,event;std::uint32_t color,event_color;};
std::uint32_t back_color_row(const BackColorRow& row,const BackColorRow* following,
                           std::uint32_t fog,std::uint32_t event9,std::uint32_t event12,bool event_camera);
std::uint32_t camera_back_color(unsigned stage,unsigned room,unsigned rcase,unsigned camera,bool event_camera,
                                std::uint32_t room_color,std::uint32_t fog,std::uint32_t event9,std::uint32_t event12);
// njSetBackColor (ps2_NaSystem.c:146): R=col>>16, G=col>>8, B=col, A=col>>24.
// blend_gs uses host alpha /255; normalize the clear alpha (original factor A/128).
// The byte host cannot retain A>128 as a destination-alpha factor; RoomWorld reports
// that explicit dependency gap while it draws over such a clear.
constexpr std::uint32_t back_color_pixel(std::uint32_t color){const auto a=color>>24;return ((a>=128?255u:(a*255u+64u)/128u)<<24)|(color&0xffffffu);}
// njSetBackColor writes Db.clear0/clear1.rgbaq (ps2_NaSystem.c:146-153), the one clear colour
// every buffer swap uses, whichever task drew the frame. Like Db it is process-wide: main.c:176
// starts it at 0, room reads (system.c:1669) and subtask switches leave it alone, and only the
// njSetBackColor call sites write it. col2/col3 are ignored by the original and are not modelled.
void set_back_color(std::uint32_t argb);
std::uint32_t installed_back_color();                                           // raw 0xAARRGGBB
inline std::uint32_t installed_back_pixel(){return back_color_pixel(installed_back_color());} // host canvas clear
}
