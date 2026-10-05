#pragma once
#include "scene.h"
namespace recvx {
Scene decode_item_asset(const std::vector<std::uint8_t>& packet,unsigned opening_frame=0,unsigned opening_item=0);
}
