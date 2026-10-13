#include "map_objects.h"
#include "binary_view.h"
#include <stdexcept>

namespace recvx {
namespace {
constexpr std::uint32_t kNone=0xffffffffu;
constexpr std::size_t kNodeSize=52;   /* NJS_CNK_OBJECT (scene.cpp:208-215) */
constexpr std::size_t kHeaderSize=16; /* MDL-Kopf: Tag, zwei Worte, obj_off   */
constexpr std::size_t kMaxNodes=4096; /* Schranke gegen beschaedigte Ketten   */

struct Walk {
    const BinaryView& view;
    std::size_t base;                 /* Zeigerbasis: Chunkanfang + 16 */
    std::uint32_t root;               /* obj_off: Anfang des Knotenfeldes */
    std::vector<MapObject>& out;

    /* Besuchsreihenfolge wie der originale Baumlauf (Kind vor Geschwister,
       scene.cpp:214f.): der Index im Ergebnis ist die Reihenfolge, in der
       bhMlbBinRealize die Knoten anlegt. */
    int visit(std::uint32_t offset){
        if(offset==kNone)return -1;
        if(out.size()>=kMaxNodes)throw std::runtime_error("map object tree too long");
        const auto node=view.relative(base,offset,kNodeSize);
        const int index=int(out.size());
        out.emplace_back();
        {
            MapObject object;
            object.pos={view.f32(node+8),view.f32(node+12),view.f32(node+16)};
            object.ang={view.i32(node+20),view.i32(node+24),view.i32(node+28)};
            /* objP->model (scene.cpp:208 node+4): MapCodeProcess case 4 draws
               only when it is not NULL. The file index of the node is the
               geometry key for the decoded MDL (scene.cpp visit: objects+i*52). */
            object.has_model=view.u32(node+4)!=kNone;
            if(offset<root||(offset-root)%kNodeSize)throw std::runtime_error("map object outside the node array");
            object.node=std::uint32_t((offset-root)/kNodeSize);
            out[std::size_t(index)]=object;
        }
        /* Erst die Kinder, dann die Geschwister; die Verweise werden nach dem
           Abstieg gesetzt, weil die Indizes erst dann feststehen. */
        const auto child=view.u32(node+44),sibling=view.u32(node+48);
        const int child_index=visit(child);
        const int sibling_index=visit(sibling);
        out[std::size_t(index)].child=child_index;
        out[std::size_t(index)].sibling=sibling_index;
        return index;
    }
};
}

std::vector<MapObject> decode_map_objects(const std::vector<std::uint8_t>& packet,const MapChunk& chunk){
    std::vector<MapObject> objects;
    if(!chunk.size||chunk.size<kHeaderSize)return objects;
    const BinaryView view(packet);
    view.require(chunk.offset,kHeaderSize);
    /* world.cpp:91: der Block traegt das MDL-Tag, obj_off steht bei +12 und
       -1 bedeutet "keine Objekte". */
    if((view.u32(chunk.offset)&0xffffffu)!=0x4c444du)return objects;
    const auto obj_off=view.u32(chunk.offset+12);
    if(obj_off==kNone)return objects;
    Walk walk{view,chunk.offset+kHeaderSize,obj_off,objects};
    walk.visit(obj_off);
    return objects;
}
}
