#include "item_asset.h"
#include "binary_view.h"
namespace recvx {
Scene decode_item_asset(const std::vector<std::uint8_t>& packet){
    BinaryView view(packet);const auto model_end=view.relative(4,view.u32(0),4);
    if((view.u32(4)&0xffffff)!=0x4c444d)throw std::runtime_error("item asset contains no MDL");
    auto textures=model_end+4;if(view.u32(model_end)&0x80000000)textures=(textures+31)&~std::size_t(31);
    view.require(textures,4);return decode_model_asset(packet,4,textures);
}
}
