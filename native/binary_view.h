#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace recvx {
class BinaryView {
public:
    explicit BinaryView(const std::vector<std::uint8_t>& data) : data_(data) {}
    void require(std::size_t offset,std::size_t count) const {
        if(offset>data_.size()||count>data_.size()-offset)throw std::runtime_error("binary field outside asset: offset "+std::to_string(offset)+", bytes "+std::to_string(count)+", asset size "+std::to_string(data_.size()));
    }
    std::uint8_t u8(std::size_t p) const {require(p,1);return data_[p];}
    std::uint16_t u16(std::size_t p) const {require(p,2);return std::uint16_t(data_[p])|(std::uint16_t(data_[p+1])<<8);}
    std::uint32_t u32(std::size_t p) const {require(p,4);return std::uint32_t(u16(p))|(std::uint32_t(u16(p+2))<<16);}
    std::int32_t i32(std::size_t p) const {auto v=u32(p);return v<=0x7fffffff?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}
    float f32(std::size_t p) const {
        static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"IEEE float required");
        const auto bits=u32(p);float f;std::memcpy(&f,&bits,4);
        if(!std::isfinite(f))throw std::runtime_error("nonfinite asset coordinate");
        return f;
    }
    std::size_t relative(std::size_t base,std::uint32_t offset,std::size_t required) const {
        require(base,0);if(offset>data_.size()-base)throw std::runtime_error("binary relative offset outside asset: base "+std::to_string(base)+", offset "+std::to_string(offset)+", asset size "+std::to_string(data_.size()));
        const auto p=base+offset;require(p,required);return p;
    }
private:
    const std::vector<std::uint8_t>& data_;
};
}
