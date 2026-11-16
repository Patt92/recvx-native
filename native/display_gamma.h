#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace recvx {
// Optional host display adjustment, not original game logic or texture decoding.
// Gamma 1 is an exact bypass; gamma >1 lifts midtones. Alpha/depth stay unchanged.
class DisplayGamma {
public:
    void set(float gamma){
        if(gamma==gamma_)return;
        gamma_=gamma;
        for(unsigned i=0;i<256;++i)table_[i]=std::uint8_t(std::lround(255.0*std::pow(double(i)/255.0,1.0/double(gamma))));
    }
    const std::uint32_t* apply(const std::vector<std::uint32_t>& source,std::vector<std::uint32_t>& output)const{
        if(gamma_==1.f)return source.data();
        output.resize(source.size());
        for(std::size_t i=0;i<source.size();++i){const auto c=source[i];output[i]=(c&0xff000000u)|(unsigned(table_[(c>>16)&255])<<16)|(unsigned(table_[(c>>8)&255])<<8)|table_[c&255];}
        return output.data();
    }
private:
    float gamma_=1.f;
    std::array<std::uint8_t,256> table_{};
};
}
