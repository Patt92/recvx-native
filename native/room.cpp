#include "room.h"
#include "expand.h"
#include "asset_io.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iomanip>
#include <sstream>
extern "C" {
#include "ps2_dvd_image.h"
}
namespace recvx {
RoomIdentity RoomIdentity::parse(const std::string& name){
    if((name.size()!=7&&name.size()!=11)||name.substr(0,3)!="RM_"||(name.size()==11&&name.substr(7)!=".RDX"&&name.substr(7)!=".RDT"))throw std::runtime_error("invalid decimal room identity");
    for(unsigned i=3;i<7;++i)if(name[i]<'0'||name[i]>'9')throw std::runtime_error("invalid decimal room identity");
    return {unsigned(name[3]-'0'),unsigned((name[4]-'0')*10+name[5]-'0'),unsigned(name[6]-'0')};
}
std::string RoomIdentity::name()const{
    if(stage>9||room>99||variant>9)throw std::runtime_error("room identity outside original filename fields");
    std::ostringstream out;out<<"RM_"<<stage<<std::setw(2)<<std::setfill('0')<<room<<variant<<".RDX";return out.str();
}
static std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset > data.size() || data.size()-offset < 4)
        throw std::runtime_error("RDT field outside buffer");
    const auto* p = data.data()+offset;
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
           (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
static float f32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559, "IEEE-754 float required");
    auto bits = u32(data, offset);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
RoomInfo inspect_room(const std::vector<std::uint8_t>& data) {
    // Layout and offsets come from bhSetRoom() and the original ROM_WORK.
    RoomInfo result{};
    result.version = f32(data, 0);
    if (!std::isfinite(result.version) || result.version < 1.72f)
        throw std::runtime_error("unsupported RDT version");
    result.room_offset = u32(data, 16);
    if (result.room_offset > data.size() || data.size()-result.room_offset < 1004)
        throw std::runtime_error("RDT room header outside buffer");
    result.model_table_offset = u32(data,20);
    result.motion_table_offset = u32(data,24);
    result.event_offset = u32(data,28);
    result.texture_offset = u32(data,32);
    for (auto offset : {result.model_table_offset, result.motion_table_offset, result.event_offset, result.texture_offset})
        if (offset && offset >= data.size()) throw std::runtime_error("RDT section outside buffer");
    const std::size_t room = result.room_offset;
    for (std::size_t i=0; i<result.counts.size(); ++i) {
        result.counts[i] = u32(data,room+0x80+i*4);
        if (result.counts[i] > data.size()) throw std::runtime_error("invalid RDT section count");
    }
    result.flags = u32(data,room+0x100);
    result.background_color = u32(data,room+0x104);
    result.fog_color = u32(data,room+0x108);
    result.fog_near = f32(data,room+0x10c);
    result.fog_far = f32(data,room+0x110);
    return result;
}
RoomArchive::RoomArchive(const std::filesystem::path& root):root_(root) {
    if(std::filesystem::is_directory(root/"rooms"))return;
    archive_=std::make_unique<AfsArchive>(disc_file(root,"RDX_LNK.AFS"));
    if (archive_->entries().size() != static_cast<std::size_t>(rdx_image_data_max))
        throw std::runtime_error("room archive does not match the US game's room table");
}
std::size_t RoomArchive::size() const { return static_cast<std::size_t>(rdx_image_data_max); }
std::string RoomArchive::name(std::size_t index) const {
    if (index >= size()) throw std::runtime_error("room index out of range");
    return rdx_files[index];
}
std::vector<std::uint8_t> RoomArchive::load(std::size_t index, std::size_t output_limit) const {
    if(index>=size())throw std::runtime_error("room index out of range");
    std::vector<std::uint8_t> data;
    if(archive_)data=expand(archive_->read(index,64*1024*1024),output_limit);
    else{auto filename=name(index);filename.replace(filename.size()-3,3,"RDT");data=read_asset_file(root_/"rooms"/filename,output_limit);}
    inspect_room(data);
    return data;
}
std::vector<std::uint8_t> RoomArchive::load(const std::string& requested, std::size_t output_limit) const {
    auto upper = requested;
    for (auto& c : upper) if (c>='a' && c<='z') c=char(c-'a'+'A');
    for (std::size_t i=0; i<size(); ++i)
        if (name(i)==upper) return load(i,output_limit);
    throw std::runtime_error("unknown room: " + requested);
}
}
