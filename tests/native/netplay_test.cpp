// Loopback checks of the host netplay transport: handshake, lockstep frame
// exchange with the shared delay, drift detection and peer loss.
#include "netplay.h"
#include <cstdlib>
#include <future>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
using namespace recvx;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static InputFrame frame_for(std::mt19937& rng){InputFrame f;f.gamepad_input=rng()&1u;for(unsigned i=0;i<action_count;++i){f.held[i]=rng()%3==0;f.pressed[i]=rng()%5==0;f.repeat[i]=rng()%7==0;}return f;}
static bool same(const InputFrame& a,const InputFrame& b){return a.gamepad_input==b.gamepad_input&&a.held==b.held&&a.pressed==b.pressed&&a.repeat==b.repeat;}
static unsigned short port_for(unsigned offset){return static_cast<unsigned short>(27000+(std::random_device{}()%2000)+offset);}
static void packing(){
    std::mt19937 rng(7);for(int i=0;i<200;++i){const auto f=frame_for(rng);check(same(Netplay::unpack(Netplay::pack(f)),f),"frame packing is not lossless");}
    std::cout<<"frame packing PASS\n";
}
static void lockstep(){
    const auto port=port_for(0);const std::string session="test-session";
    auto host_future=std::async(std::launch::async,[&]{return Netplay::host(port,session,2,10);});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto guest=Netplay::join("127.0.0.1",port,session,2,10);auto host=host_future.get();
    check(host->role()==Netplay::Role::Host&&guest->role()==Netplay::Role::Guest&&host->connected()&&guest->connected(),"handshake roles");
    std::mt19937 rh(1),rg(2);std::vector<InputFrame> host_sent,guest_sent;
    for(unsigned tick=0;tick<120;++tick){
        const auto h=frame_for(rh),g=frame_for(rg);host_sent.push_back(h);guest_sent.push_back(g);
        host->submit(tick,h);guest->submit(tick,g);
        for(int spin=0;spin<2000&&!(host->ready(tick)&&guest->ready(tick));++spin)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(host->ready(tick)&&guest->ready(tick),"lockstep tick not ready");
        // Both peers apply the same two frames for this tick: the ones sampled `delay` ticks earlier (empty before that).
        const InputFrame expect_h=tick>=2?host_sent[tick-2]:InputFrame{},expect_g=tick>=2?guest_sent[tick-2]:InputFrame{};
        check(same(host->local(tick),expect_h)&&same(guest->remote(tick),expect_h),"host frame differs between peers");
        check(same(guest->local(tick),expect_g)&&same(host->remote(tick),expect_g),"guest frame differs between peers");
        if(tick%15==0){host->hash(tick,0x1234ull+tick);guest->hash(tick,0x1234ull+tick);}
    }
    for(int spin=0;spin<200;++spin){host->ready(120);guest->ready(120);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(!host->desynced()&&!guest->desynced(),"equal hashes reported as desync");
    host->hash(135,{1,5,7,9});guest->hash(135,{2,5,8,9});
    for(int spin=0;spin<2000&&!(host->desynced()&&guest->desynced());++spin){host->ready(121);guest->ready(121);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(host->desynced()&&guest->desynced()&&host->desync_tick()==135&&guest->desync_tick()==135&&host->desync_parts()==0b0101u&&guest->desync_parts()==0b0101u&&host->status().find("player 1/room, enemies")!=std::string::npos,"hash mismatch/parts not detected on both peers");
    {   // Rejoin: the host's save image travels as one message and arrives whole.
        std::vector<std::uint8_t> image(70000);for(std::size_t i=0;i<image.size();++i)image[i]=std::uint8_t(i*7+3);
        host->send_snapshot(image);std::vector<std::uint8_t> got;
        for(int spin=0;spin<4000&&!guest->take_snapshot(got);++spin)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(got==image,"snapshot image did not arrive intact");
        check(!guest->take_snapshot(got),"snapshot image delivered twice");
    }
    host->forget_before(100);check(same(host->local(119),host_sent[117]),"forget_before dropped a kept frame");
    guest.reset();
    for(int spin=0;spin<2000&&host->connected();++spin){host->ready(121);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    // Frames already received stay usable (the runtime drains them), only the connection flag drops.
    check(!host->connected()&&host->ready(121)&&!host->ready(122),"peer loss not detected");
    std::cout<<"lockstep exchange, delay, drift detection and peer loss PASS\n";
}
static void refused_session(){
    const auto port=port_for(1);
    auto host_future=std::async(std::launch::async,[&]{try{Netplay::host(port,"a",2,10);return false;}catch(const std::exception&){return true;}});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bool guest_refused=false;try{Netplay::join("127.0.0.1",port,"b",2,10);}catch(const std::exception& e){guest_refused=std::string(e.what()).find("session differs")!=std::string::npos;}
    check(guest_refused&&host_future.get(),"different sessions were not refused");
    bool delay_refused=false;const auto port2=port_for(2);auto host2=std::async(std::launch::async,[&]{try{Netplay::host(port2,"a",2,10);return false;}catch(const std::exception&){return true;}});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    try{Netplay::join("127.0.0.1",port2,"a",3,10);}catch(const std::exception& e){delay_refused=std::string(e.what()).find("delay differs")!=std::string::npos;}
    check(delay_refused&&host2.get(),"different input delays were not refused");
    std::cout<<"session/delay handshake refusal PASS\n";
}
static void lobby(){
    for(const auto* address:{"0.0.0.0:1","127.0.0.1:57341","192.168.178.200:65535","255.255.255.255:49152"})check(Netplay::endpoint(Netplay::phrase(address))==Netplay::endpoint(address),"phrase did not round-trip IP and port");
    for(const auto* bad:{"127.0.0.1:0","127.0.0.1:65536","127.0.0.1:1x","256.1.1.1","amberbird amberbird","1.2.3.4:"}){bool refused=false;try{Netplay::endpoint(bad);}catch(const std::exception&){refused=true;}check(refused,"malformed endpoint accepted");}
    std::atomic_bool cancel{false};const auto port=port_for(10);
    auto waiting=std::async(std::launch::async,[&]{try{Netplay::host(port,"cancel",2,120,&cancel);return false;}catch(const std::exception&){return true;}});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));cancel=true;
    check(waiting.wait_for(std::chrono::seconds(1))==std::future_status::ready&&waiting.get(),"lobby cancellation did not release listener");
    auto host=std::async(std::launch::async,[&]{return Netplay::host(port,"seats",2,5,nullptr,true);});
    auto guest=Netplay::join("127.0.0.1",port,"seats",2,5);auto h=host.get();check(!h->local_player1()&&guest->local_player1(),"host P2 assignment not negotiated");
    std::cout<<"Lobby address, validation, cancellation, port reuse and player assignment PASS\n";
}
int main(){
    try{lobby();packing();lockstep();refused_session();}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
    return 0;
}
