#include "system_data.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace recvx {
std::uint32_t memory_card_sum(const std::uint8_t* data,std::size_t size){
    std::uint32_t sum=0;for(std::size_t i=0;i<size;++i)sum+=data[i];return sum;
}
SystemData SystemData::create_init(){SystemData d;d.vibration=1;return d;}
std::vector<std::uint8_t> SystemData::encode()const{
    std::vector<std::uint8_t> out;out.reserve(size);
    auto u32=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)out.push_back(std::uint8_t(v>>(8*i)));};
    u32(ssd_ver);u32(ssd_flg);u32(ssd_reserve);
    out.push_back(std::uint8_t(keytype));out.push_back(std::uint8_t(adjust_x));out.push_back(std::uint8_t(adjust_y));out.push_back(std::uint8_t(vibration));
    for(auto t:best_tm)u32(std::uint32_t(t));
    u32(memory_card_sum(out.data(),out.size()));
    return out;
}
SystemData SystemData::decode(const std::vector<std::uint8_t>& bytes){
    if(bytes.size()!=size)throw std::runtime_error("system data: CONFIGFILE size");
    auto u32=[&](std::size_t p){return std::uint32_t(bytes[p])|std::uint32_t(bytes[p+1])<<8|std::uint32_t(bytes[p+2])<<16|std::uint32_t(bytes[p+3])<<24;};
    if(u32(0x30)!=memory_card_sum(bytes.data(),0x30))throw std::runtime_error("system data: Check_Sam mismatch");
    SystemData d;d.ssd_ver=u32(0);d.ssd_flg=u32(4);d.ssd_reserve=u32(8);
    d.keytype=std::int8_t(bytes[0xc]);d.adjust_x=std::int8_t(bytes[0xd]);d.adjust_y=std::int8_t(bytes[0xe]);d.vibration=std::int8_t(bytes[0xf]);
    for(unsigned i=0;i<8;++i)d.best_tm[i]=std::int32_t(u32(0x10+4*i));
    return d;
}
void sys_save_hikaku(SystemData& sys,SystemData& file){
    if(file.ssd_flg!=0){
        sys.ssd_flg|=file.ssd_flg;
        for(unsigned i=0;i<8;++i)if(sys.best_tm[i]>file.best_tm[i]&&file.best_tm[i]!=0)sys.best_tm[i]=file.best_tm[i];
    }else for(unsigned i=0;i<8;++i)sys.best_tm[i]=file.best_tm[i]=0;
}
std::filesystem::path system_data_path(const std::filesystem::path& saves_dir){return saves_dir/"system.rcvxsys";}
SystemDataRead read_system_data(const std::filesystem::path& saves_dir,SystemData& out){
    const auto path=system_data_path(saves_dir);std::error_code ec;
    if(!std::filesystem::is_regular_file(path,ec))return SystemDataRead::Missing;
    std::ifstream in(path,std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    try{out=SystemData::decode(bytes);}catch(const std::exception&){return SystemDataRead::Damaged;}
    return SystemDataRead::Ok;
}
void write_system_data(const std::filesystem::path& saves_dir,const SystemData& data){
    std::filesystem::create_directories(saves_dir);
    const auto path=system_data_path(saves_dir);auto tmp=path;tmp+=".tmp";
    const auto bytes=data.encode();
    {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!out)throw std::runtime_error("system data: write failed");}
    std::filesystem::rename(tmp,path);
}
}
