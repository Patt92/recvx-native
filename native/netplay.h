#pragma once
// Host feature (user request 2026-11-04): peer-to-peer two-player session.
// Deterministic lockstep over one TCP connection: both peers run the same
// native simulation and exchange the per-tick InputFrame of their local
// player; the host selects P1/P2 and the guest receives the other seat. No
// game state travels over the wire; a periodic state hash detects drift.
// This is a host transport only, not an original mechanism.
#include "input.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace recvx {
class Netplay {
public:
    enum class Role {Host,Guest};
    // Blocking: listens on `port` until a peer connects (`timeout_seconds`) and completes the handshake.
    static std::unique_ptr<Netplay> host(unsigned short port,const std::string& session,unsigned delay,double timeout_seconds,const std::atomic_bool* cancel=nullptr,bool host_player2=false);
    // Blocking: connects to host:port and completes the handshake.
    static std::unique_ptr<Netplay> join(const std::string& address,unsigned short port,const std::string& session,unsigned delay,double timeout_seconds,const std::atomic_bool* cancel=nullptr);
    ~Netplay();
    Netplay(const Netplay&)=delete;Netplay& operator=(const Netplay&)=delete;
    Role role()const{return role_;}
    bool local_player1()const{return (role_==Role::Host)!=host_player2_;}
    static constexpr unsigned short default_port=57341;
    static std::string phrase(const std::string& endpoint);
    static std::pair<std::string,unsigned short> endpoint(const std::string& text);
    static std::string local_address();
    unsigned delay()const{return delay_;}
    // The frame sampled at `tick` is the input of tick + delay on both peers; sent at once. Idempotent per tick.
    void submit(unsigned tick,const InputFrame& frame);
    // Pumps the socket. True when both peers' frames for `tick` are known.
    bool ready(unsigned tick);
    InputFrame local(unsigned tick)const;
    InputFrame remote(unsigned tick)const;
    // Periodic drift check: both peers hash the same state after `tick`; a mismatch sets desync_tick().
    // Four part hashes (player 1/room, player 2, enemies, screens/flags) name the drifting part in status().
    static constexpr unsigned parts=4;
    void hash(unsigned tick,const std::array<std::uint64_t,parts>& value);
    void hash(unsigned tick,std::uint64_t value){hash(tick,std::array<std::uint64_t,parts>{value,0,0,0});}
    bool desynced()const{return desync_tick_>=0;}
    long desync_tick()const{return desync_tick_;}
    unsigned desync_parts()const{return desync_parts_;} // bit i = part i differs
    bool connected()const{return connected_;}
    const std::string& error()const{return error_;}
    std::string status()const;
    // Drops frames/hashes older than `tick`.
    void forget_before(unsigned tick);
    // Wire helpers (also for tests): 32-bit action masks of an InputFrame.
    struct Packed {std::uint32_t held=0,pressed=0,repeat=0;std::uint8_t gamepad=0;};
    static Packed pack(const InputFrame& frame);
    static InputFrame unpack(const Packed& packed);
private:
    Netplay(Role role,std::intptr_t socket,unsigned delay);
    void pump();
    bool send_bytes(const std::vector<std::uint8_t>& bytes);
    void fail(const std::string& why);
    bool host_player2_=false;Role role_;std::intptr_t socket_;unsigned delay_;
    std::map<unsigned,Packed> local_,remote_;
    std::map<unsigned,std::array<std::uint64_t,parts>> local_hash_,remote_hash_;unsigned desync_parts_=0;
    std::vector<std::uint8_t> inbox_;
    bool connected_=true;long desync_tick_=-1;std::string error_;
    unsigned sent_=0,received_=0;
};
}
