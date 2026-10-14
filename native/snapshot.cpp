#include "snapshot.h"
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace recvx {
namespace {
constexpr char magic[8]={'R','C','V','X','S','N','A','P'};
constexpr std::uint32_t format=1;
struct Writer {
    std::vector<std::uint8_t> out;
    void u8(std::uint32_t v){out.push_back(std::uint8_t(v));}
    void u16(std::uint32_t v){u8(v);u8(v>>8);}
    void u32(std::uint32_t v){u16(v);u16(v>>16);}
    void f32(float v){std::uint32_t bits;std::memcpy(&bits,&v,4);u32(bits);}
    template<class T,std::size_t N>void words(const std::array<T,N>& a){for(auto v:a)u32(v);}
};
struct Reader {
    const std::vector<std::uint8_t>& in;std::size_t at=0;
    void need(std::size_t n)const{if(at+n>in.size())throw std::runtime_error("snapshot truncated");}
    std::uint32_t u8(){need(1);return in[at++];}
    std::uint32_t u16(){const auto lo=u8();return lo|(u8()<<8);}
    std::uint32_t u32(){const auto lo=u16();return lo|(u16()<<16);}
    float f32(){const auto bits=u32();float v;std::memcpy(&v,&bits,4);return v;}
    template<class T,std::size_t N>void words(std::array<T,N>& a){for(auto& v:a)v=T(u32());}
};
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(int k=0;k<8;++k)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}
    return ~crc;
}
constexpr unsigned session_matrices[4]={11,12,15,16};
constexpr unsigned session_values[3]={7,18,25};
template<std::size_t N>void to_save(std::array<std::uint32_t,N>& to,const std::array<std::uint32_t,32>& from){for(std::size_t i=0;i<N;++i)to[i]=from[i];}
template<std::size_t N>void from_save(std::array<std::uint32_t,32>& to,const std::array<std::uint32_t,N>& from){to.fill(0);for(std::size_t i=0;i<N;++i)to[i]=from[i];}
}

std::vector<std::uint8_t> SaveFile::encode()const{
    Writer w;w.u32(version);w.u32(std::uint32_t(save_ct));
    for(auto v:{ply_id,stg_no,rom_no,rcase,pos_no,cut_no,flr_no,reserve0})w.u8(std::uint8_t(v));
    w.u32(std::uint32_t(gm_mode));
    w.words(ev_flg);w.words(it_flg);w.words(ic_flg);w.words(ed_flg);w.words(ky_flg);w.words(mp_flg);w.words(itm);w.words(ply_stflg);
    w.f32(ply_pos.x);w.f32(ply_pos.y);w.f32(ply_pos.z);w.u32(std::uint32_t(ply_ang));
    for(auto v:ply_wno)w.u8(std::uint8_t(v));
    for(auto v:ply_hp)w.u16(std::uint16_t(v));
    for(auto v:evt_posno)w.u8(v);
    w.u32(std::uint32_t(time));w.u32(std::uint32_t(stv_tm));
    for(auto v:{spray_ct,retry_ct,clear_ct,reserve1})w.u16(std::uint16_t(v));
    w.u32(check_sum);
    if(w.out.size()!=size)throw std::logic_error("SAVEFILE layout mismatch");
    return w.out;
}
SaveFile SaveFile::decode(const std::vector<std::uint8_t>& bytes){
    if(bytes.size()!=size)throw std::runtime_error("SAVEFILE must be 0x838 bytes");
    Reader r{bytes};SaveFile s;s.version=r.u32();s.save_ct=std::int32_t(r.u32());
    for(auto* v:{&s.ply_id,&s.stg_no,&s.rom_no,&s.rcase,&s.pos_no,&s.cut_no,&s.flr_no,&s.reserve0})*v=std::int8_t(r.u8());
    s.gm_mode=std::int32_t(r.u32());
    r.words(s.ev_flg);r.words(s.it_flg);r.words(s.ic_flg);r.words(s.ed_flg);r.words(s.ky_flg);r.words(s.mp_flg);r.words(s.itm);r.words(s.ply_stflg);
    s.ply_pos.x=r.f32();s.ply_pos.y=r.f32();s.ply_pos.z=r.f32();s.ply_ang=std::int32_t(r.u32());
    for(auto& v:s.ply_wno)v=std::int8_t(r.u8());
    for(auto& v:s.ply_hp)v=std::int16_t(r.u16());
    for(auto& v:s.evt_posno)v=std::uint8_t(r.u8());
    s.time=std::int32_t(r.u32());s.stv_tm=std::int32_t(r.u32());
    for(auto* v:{&s.spray_ct,&s.retry_ct,&s.clear_ct,&s.reserve1})*v=std::int16_t(r.u16());
    s.check_sum=r.u32();
    return s;
}

Snapshot capture_snapshot(const Inventory& inventory,const EventVm& events,unsigned body,const std::string& room,const SnapshotPlayer& player){
    Snapshot snap;auto& s=snap.save;
    if(inventory.player>3)throw std::runtime_error("player id outside SAVEFILE");
    s.ply_id=std::int8_t(inventory.player);s.stg_no=std::int8_t(events.values[0]);s.rom_no=std::int8_t(events.values[1]);s.rcase=std::int8_t(events.values[21]);
    s.pos_no=std::int8_t(player.spawn);s.cut_no=std::int8_t(player.camera);s.flr_no=std::int8_t(player.floor);s.gm_mode=std::int32_t(inventory.game_mode);s.save_ct=inventory.save_ct;
    to_save(s.ev_flg,events.flags[1]);to_save(s.ky_flg,events.flags[2]);to_save(s.ed_flg,events.flags[3]);
    to_save(s.it_flg,events.flags[7]);to_save(s.mp_flg,events.flags[8]);to_save(s.ic_flg,events.flags[9]);
    s.itm=inventory.items;
    // room.c:1033 npCopyMemory kopiert den ganzen SYS-Bereich, also auch die
    // gespeicherten Eintraege der anderen Spieler in ply_stflg/ply_wno/ply_hp
    // (system.c:1729/1746/1747 bhFinishRoom bei bhPlayerChangeSet). Nur der
    // aktive Eintrag wird davor aus plp neu geschrieben (room.c:1022/1030/1031).
    for(std::size_t i=0;i<s.ply_hp.size();++i){s.ply_hp[i]=std::int16_t(inventory.ply_hp[i]);s.ply_stflg[i]=inventory.ply_stflg[i];s.ply_wno[i]=std::int8_t(inventory.partner_weapon[i]);}
    // bhPushGameData: sys->ply_stflg[ply_id] = plp->stflg & 0x78280000.
    s.ply_stflg[s.ply_id]=events.flags[14][0]&0x78280000u;
    s.ply_pos=player.position;s.ply_ang=player.yaw;
    s.ply_wno[s.ply_id]=std::int8_t(inventory.weapon());s.ply_hp[s.ply_id]=std::int16_t(inventory.hp);
    s.stv_tm=std::int32_t(inventory.play_time);
    auto& h=snap.host;h.room=room;h.body=body;
    for(unsigned i=0;i<4;++i)h.session_flags[i]=events.flags[session_matrices[i]];
    for(unsigned i=0;i<3;++i)h.session_values[i]=events.values[session_values[i]];
    h.max_hp=inventory.max_hp;h.poisoned=inventory.poisoned;h.infected=inventory.infected;h.side_pack=inventory.side_pack;
    return snap;
}
void restore_inventory(const Snapshot& snap,Inventory& inventory){
    const auto& s=snap.save;if(s.ply_id<0||s.ply_id>3)throw std::runtime_error("snapshot player id invalid");
    inventory.items=s.itm;inventory.player=unsigned(s.ply_id);inventory.game_mode=unsigned(s.gm_mode);inventory.save_ct=s.save_ct;
    inventory.hp=s.ply_hp[s.ply_id];inventory.max_hp=snap.host.max_hp;inventory.play_time=std::uint32_t(s.stv_tm);
    // bhPopGameData restores sys->ply_hp / sys->ply_stflg together with the pip
    // bank. Keep the per-player arrays in step with the live player so a later
    // bhPlayerChangeSet saves and restores the correct values. ply_stflg holds
    // the same poison bits as player_stflg() (0x80000/0x200000).
    for(std::size_t i=0;i<inventory.ply_hp.size();++i){inventory.ply_hp[i]=s.ply_hp[i];inventory.ply_stflg[i]=unsigned(s.ply_stflg[i])&0x280000u;inventory.partner_weapon[i]=std::uint8_t(s.ply_wno[i]);} // room.c:1043 auch sys->ply_wno
    inventory.poisoned=snap.host.poisoned;inventory.infected=snap.host.infected;inventory.side_pack=snap.host.side_pack;
}
void restore_events(const Snapshot& snap,EventVm& events){
    const auto& s=snap.save;
    from_save(events.flags[1],s.ev_flg);from_save(events.flags[2],s.ky_flg);from_save(events.flags[3],s.ed_flg);
    from_save(events.flags[7],s.it_flg);from_save(events.flags[8],s.mp_flg);from_save(events.flags[9],s.ic_flg);
    for(unsigned i=0;i<4;++i)events.flags[session_matrices[i]]=snap.host.session_flags[i];
    for(unsigned i=0;i<3;++i)events.values[session_values[i]]=snap.host.session_values[i];
    // room.c bhSetRoom with cb_flg 0x800000: plp->stflg = ply_stflg | 0x40000000.
    events.flags[14][0]=s.ply_stflg[s.ply_id]|0x40000000u;
}
SnapshotPlayer snapshot_player(const Snapshot& snap){
    const auto& s=snap.save;return {s.ply_pos,Angle(s.ply_ang),s.flr_no,unsigned(std::uint8_t(s.pos_no)),std::size_t(std::uint8_t(s.cut_no))};
}

void write_snapshot(const std::filesystem::path& path,const Snapshot& snap){
    Writer w;for(char c:magic)w.u8(std::uint8_t(c));w.u32(format);
    const auto save=snap.save.encode();w.u32(std::uint32_t(save.size()));w.out.insert(w.out.end(),save.begin(),save.end());
    const auto& h=snap.host;w.u32(std::uint32_t(h.room.size()));for(char c:h.room)w.u8(std::uint8_t(c));
    w.u32(h.body);for(const auto& m:h.session_flags)w.words(m);for(auto v:h.session_values)w.u32(v);
    w.u32(std::uint32_t(h.max_hp));w.u8(h.poisoned);w.u8(h.infected);w.u8(h.side_pack);
    w.u32(crc32(w.out.data(),w.out.size()));
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=".tmp";
    {std::ofstream file(temporary,std::ios::binary);if(!file)throw std::runtime_error("cannot write snapshot "+temporary.string());
     file.write(reinterpret_cast<const char*>(w.out.data()),std::streamsize(w.out.size()));file.close();if(!file)throw std::runtime_error("snapshot write failed");}
    std::filesystem::rename(temporary,path);
}
Snapshot read_snapshot(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("snapshot not found: "+path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(bytes.size()<16||std::memcmp(bytes.data(),magic,8))throw std::runtime_error("not a RECVX snapshot");
    Reader tail{bytes,bytes.size()-4};if(tail.u32()!=crc32(bytes.data(),bytes.size()-4))throw std::runtime_error("snapshot checksum mismatch");
    bytes.resize(bytes.size()-4);Reader r{bytes,8};
    if(r.u32()!=format)throw std::runtime_error("unsupported snapshot format");
    if(r.u32()!=SaveFile::size)throw std::runtime_error("snapshot SAVEFILE size mismatch");
    r.need(SaveFile::size);Snapshot snap;snap.save=SaveFile::decode({bytes.begin()+std::ptrdiff_t(r.at),bytes.begin()+std::ptrdiff_t(r.at+SaveFile::size)});r.at+=SaveFile::size;
    auto& h=snap.host;const auto length=r.u32();if(length>64)throw std::runtime_error("snapshot room name invalid");r.need(length);h.room.assign(bytes.begin()+std::ptrdiff_t(r.at),bytes.begin()+std::ptrdiff_t(r.at+length));r.at+=length;
    h.body=r.u32();for(auto& m:h.session_flags)r.words(m);for(auto& v:h.session_values)v=r.u32();
    h.max_hp=int(r.u32());h.poisoned=r.u8();h.infected=r.u8();h.side_pack=r.u8();
    if(r.at!=bytes.size())throw std::runtime_error("snapshot has trailing data");
    if(snap.save.version!=10)throw std::runtime_error("snapshot SAVEFILE version is not 10");
    return snap;
}
}
