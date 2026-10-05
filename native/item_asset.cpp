#include "item_asset.h"
#include "binary_view.h"
#include <algorithm>
namespace recvx {
Scene decode_item_asset(const std::vector<std::uint8_t>& packet,unsigned opening_frame,unsigned opening_item){
    BinaryView view(packet);const auto model_end=view.relative(4,view.u32(0),4);
    if((view.u32(4)&0xffffff)!=0x4c444d)throw std::runtime_error("item asset contains no MDL");
    auto textures=model_end+4;if(view.u32(model_end)&0x80000000)textures=(textures+31)&~std::size_t(31);
    view.require(textures,4);if(!opening_item)return decode_model_asset(packet,4,textures);
    const auto base=view.relative(4,view.u16(8),0);const auto count=unsigned(view.u16(10));if(!count||count>256)throw std::runtime_error("invalid inspection model node count");
    const auto nodes=view.relative(base,view.u32(16),count*52);if(nodes+count*52>model_end)throw std::runtime_error("inspection nodes outside model");
    ModelGeometryOptions options;for(unsigned i=0;i<count;++i){const auto p=nodes+i*52;options.pose.push_back({{view.f32(p+8),view.f32(p+12),view.f32(p+16)},{view.i32(p+20),view.i32(p+24),view.i32(p+28)}});}
    // Original ItemView::FlagErase enables local translation/rotation. The
    // original case action rotates the lid and its catch on the child chain.
    if(opening_frame&&(opening_item==83||opening_item==63||opening_item==128||opening_item==129)){
        auto child=[&](unsigned node){const auto off=view.u32(nodes+node*52+44);const auto at=view.relative(base,off,52);if(at<nodes||(at-nodes)%52||(at-nodes)/52>=count)throw std::runtime_error("case action child outside model");return unsigned((at-nodes)/52);};
        const auto frames=std::min(opening_frame,23u);
        if(count==3){
            // PS2 ITEM1 uses a dummy root, case body and hinged lid; the
            // separately animated catch in the decompiled hierarchy is absent.
            const auto lid=child(child(0));options.pose[lid].angles[0]-=frames*728;
        }else{
            const auto lid=child(0);auto latch=lid;for(unsigned i=1;i<(opening_item==128?5u:opening_item==63?4u:3u);++i)latch=child(latch);
            options.pose[lid].angles[0]-=frames*728;options.pose[latch].angles[0]+=frames*1456;
        }
    }
    return decode_model_asset(packet,4,textures,options);
}
}
