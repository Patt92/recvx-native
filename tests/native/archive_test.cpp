#include "archive.h"
#include "room.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
static void check(bool value) { if (!value) throw std::runtime_error("archive/room assertion failed"); }
static void word(Bytes& b,std::size_t i,std::uint32_t value) {
    for (int n=0; n<4; ++n) b.at(i+n)=static_cast<std::uint8_t>(value>>(n*8));
}
static void real(Bytes& b,std::size_t i,float value) {
    std::uint32_t bits; std::memcpy(&bits,&value,4); word(b,i,bits);
}
template<class F> static void rejects(F call) {
    try { call(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("malformed data accepted");
}
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/ ("recvx-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directory(root);
        const auto path=root/"fixture.afs";
        auto save=[&](const Bytes& b) {
            std::ofstream f(path,std::ios::binary|std::ios::trunc);
            f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
        };
        Bytes b(64); b[0]='A';b[1]='F';b[2]='S';
        word(b,4,2);word(b,8,32);word(b,12,3);word(b,16,40);word(b,20,0);
        b[32]=1;b[33]=2;b[34]=3;save(b);
        recvx::AfsArchive a(path);
        check(a.entries().size()==2 && a.read(0,3)==Bytes({1,2,3}));
        check(a.read(1,0).empty());
        check(recvx::disc_file(root,"FIXTURE.AFS")==path);
        rejects([&]{a.read(2,10);}); rejects([&]{a.read(0,2);});
        rejects([&]{recvx::disc_file(root,"../fixture.afs");});
        b[3]=' ';save(b);check(recvx::AfsArchive(path).entries().size()==2);
        b[3]=1;save(b);rejects([&]{recvx::AfsArchive invalid(path);});
        b[3]=0;word(b,12,40);save(b);rejects([&]{recvx::AfsArchive invalid(path);});
        word(b,12,3);word(b,8,4);save(b);rejects([&]{recvx::AfsArchive invalid(path);});
        word(b,4,1000001);save(b);rejects([&]{recvx::AfsArchive invalid(path);});
        b.resize(4);save(b);rejects([&]{recvx::AfsArchive invalid(path);});
        fs::remove(path);rejects([&]{a.read(1,0);});
        Bytes room(1200);real(room,0,10);word(room,16,128);
        word(room,128+0x80,3);word(room,128+0x88,2);
        const auto info=recvx::inspect_room(room);
        check(info.version==10 && info.counts[0]==3 && info.counts[2]==2);
        word(room,20,1200);rejects([&]{recvx::inspect_room(room);});word(room,20,0);
        real(room,0,1);rejects([&]{recvx::inspect_room(room);});real(room,0,10);
        word(room,16,1200);rejects([&]{recvx::inspect_room(room);});
        rejects([&]{recvx::inspect_room({});});
        fs::remove_all(root);
        std::cout << "Archive and room tests passed\n";
    } catch (const std::exception& e) { fs::remove_all(root);std::cerr << e.what() << '\n';return 1; }
}
