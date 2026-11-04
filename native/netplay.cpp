#include "netplay.h"
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define RECVX_INVALID_SOCKET INVALID_SOCKET
#define RECVX_SOCKET_ERROR SOCKET_ERROR
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#define RECVX_INVALID_SOCKET (-1)
#define RECVX_SOCKET_ERROR (-1)
#endif
namespace recvx {
namespace {
thread_local const std::atomic_bool* cancellation=nullptr;
bool cancelled(){return cancellation&&cancellation->load();}

#if defined(_WIN32)
using Socket=SOCKET;
struct WinsockInit{WinsockInit(){WSADATA d;WSAStartup(MAKEWORD(2,2),&d);}~WinsockInit(){WSACleanup();}};
void ensure_winsock(){static WinsockInit init;(void)init;}
void close_socket(Socket s){closesocket(s);}
bool would_block(){const int e=WSAGetLastError();return e==WSAEWOULDBLOCK;}
void set_nonblocking(Socket s){u_long mode=1;ioctlsocket(s,FIONBIO,&mode);}
#else
using Socket=int;
void ensure_winsock(){}
void close_socket(Socket s){::close(s);}
bool would_block(){return errno==EWOULDBLOCK||errno==EAGAIN;}
void set_nonblocking(Socket s){const int flags=fcntl(s,F_GETFL,0);fcntl(s,F_SETFL,flags|O_NONBLOCK);}
#endif
void set_nodelay(Socket s){int one=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&one),sizeof one);
#if defined(SO_NOSIGPIPE)
    setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,reinterpret_cast<const char*>(&one),sizeof one); /* a peer that left must not kill the process with SIGPIPE */
#endif
}
#if defined(MSG_NOSIGNAL)
constexpr int send_flags=MSG_NOSIGNAL;
#else
constexpr int send_flags=0;
#endif
constexpr std::uint32_t magic=0x58564352u; // "RCVX"
constexpr std::uint32_t protocol=3; // 2: four part hashes
enum Message:std::uint8_t {Input=1,Hash=2,Bye=3};
void put32(std::vector<std::uint8_t>& out,std::uint32_t v){for(int i=0;i<4;++i)out.push_back(std::uint8_t(v>>(8*i)));}
void put64(std::vector<std::uint8_t>& out,std::uint64_t v){for(int i=0;i<8;++i)out.push_back(std::uint8_t(v>>(8*i)));}
std::uint32_t get32(const std::uint8_t* p){std::uint32_t v=0;for(int i=0;i<4;++i)v|=std::uint32_t(p[i])<<(8*i);return v;}
std::uint64_t get64(const std::uint8_t* p){std::uint64_t v=0;for(int i=0;i<8;++i)v|=std::uint64_t(p[i])<<(8*i);return v;}
// Blocking exchange of the handshake with a deadline; both sides send first, then read.
void send_all(Socket s,const std::vector<std::uint8_t>& bytes,std::chrono::steady_clock::time_point deadline){
    std::size_t done=0;while(done<bytes.size()){const auto n=::send(s,reinterpret_cast<const char*>(bytes.data()+done),int(bytes.size()-done),send_flags);
        if(n>0){done+=std::size_t(n);continue;}if(n==RECVX_SOCKET_ERROR&&would_block()){if((cancelled()||std::chrono::steady_clock::now()>deadline))throw std::runtime_error("netplay handshake timed out");std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
        throw std::runtime_error("netplay handshake send failed");}}
std::vector<std::uint8_t> recv_exact(Socket s,std::size_t count,std::chrono::steady_clock::time_point deadline){
    std::vector<std::uint8_t> out(count);std::size_t done=0;while(done<count){const auto n=::recv(s,reinterpret_cast<char*>(out.data()+done),int(count-done),0);
        if(n>0){done+=std::size_t(n);continue;}if(n==0)throw std::runtime_error("netplay peer closed during handshake");
        if(would_block()){if((cancelled()||std::chrono::steady_clock::now()>deadline))throw std::runtime_error("netplay handshake timed out");std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
        throw std::runtime_error("netplay handshake receive failed");}return out;}
bool handshake(Socket s,const std::string& session,unsigned delay,double timeout_seconds,bool host,bool player2=false){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(long(timeout_seconds*1000));
    std::vector<std::uint8_t> hello;put32(hello,magic);put32(hello,protocol);put32(hello,delay);put32(hello,std::uint32_t(session.size()));hello.insert(hello.end(),session.begin(),session.end());
    send_all(s,hello,deadline);
    const auto head=recv_exact(s,16,deadline);
    if(get32(head.data())!=magic)throw std::runtime_error("netplay peer is not a RECVX runtime");
    if(get32(head.data()+4)!=protocol)throw std::runtime_error("netplay protocol version differs (peer "+std::to_string(get32(head.data()+4))+", mine "+std::to_string(protocol)+")");
    if(get32(head.data()+8)!=delay)throw std::runtime_error("netplay input delay differs (peer "+std::to_string(get32(head.data()+8))+", mine "+std::to_string(delay)+")");
    const auto length=get32(head.data()+12);if(length>4096)throw std::runtime_error("netplay session description too long");
    const auto body=recv_exact(s,length,deadline);const std::string theirs(body.begin(),body.end());
    if(theirs!=session)throw std::runtime_error("netplay session differs: peer runs \""+theirs+"\", mine is \""+session+"\"");
    send_all(s,{std::uint8_t(host?1u|(player2?2u:0u):0u)},deadline);
    const auto seat=recv_exact(s,1,deadline)[0];
    if(host?seat!=0:(seat!=1&&seat!=3))throw std::runtime_error("netplay: invalid player assignment");
    return host?player2:(seat&2)!=0;
}
Socket make_socket(){ensure_winsock();const Socket s=::socket(AF_INET,SOCK_STREAM,0);if(s==RECVX_INVALID_SOCKET)throw std::runtime_error("netplay: socket() failed");return s;}
}
Netplay::Netplay(Role role,std::intptr_t socket,unsigned delay):role_(role),socket_(socket),delay_(delay){
    // Ticks before the first delayed sample have no input on either side.
    for(unsigned t=0;t<delay_;++t){local_[t]={};remote_[t]={};}
}
Netplay::~Netplay(){if(socket_!=std::intptr_t(RECVX_INVALID_SOCKET)){std::vector<std::uint8_t> bye{Bye};(void)send_bytes(bye);close_socket(Socket(socket_));}}
std::unique_ptr<Netplay> Netplay::host(unsigned short port,const std::string& session,unsigned delay,double timeout_seconds,const std::atomic_bool* cancel,bool host_player2){
    cancellation=cancel;
    const Socket listener=make_socket();
#if defined(_WIN32)
    int one=1;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&one),sizeof one);
#endif
    // No SO_REUSEADDR: on macOS it permits a wildcard lobby alongside an
    // existing interface-specific listener, which would receive that IP's guests.
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons(port);
    if(::bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof address)==RECVX_SOCKET_ERROR){close_socket(listener);throw std::runtime_error("netplay: cannot listen on port "+std::to_string(port));}
    if(::listen(listener,1)==RECVX_SOCKET_ERROR){close_socket(listener);throw std::runtime_error("netplay: listen failed");}
    set_nonblocking(listener);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(long(timeout_seconds*1000));
    Socket peer=RECVX_INVALID_SOCKET;
    while(peer==RECVX_INVALID_SOCKET){sockaddr_in from{};socklen_t length=sizeof from;peer=::accept(listener,reinterpret_cast<sockaddr*>(&from),&length);
        if(peer==RECVX_INVALID_SOCKET){if((cancelled()||std::chrono::steady_clock::now()>deadline)){close_socket(listener);throw std::runtime_error("netplay: no peer connected within the timeout");}std::this_thread::sleep_for(std::chrono::milliseconds(20));}}
    close_socket(listener);set_nodelay(peer);set_nonblocking(peer);
    try{handshake(peer,session,delay,timeout_seconds,true,host_player2);}catch(...){close_socket(peer);throw;}
    auto result=std::unique_ptr<Netplay>(new Netplay(Role::Host,std::intptr_t(peer),delay));result->host_player2_=host_player2;return result;
}
std::unique_ptr<Netplay> Netplay::join(const std::string& address,unsigned short port,const std::string& session,unsigned delay,double timeout_seconds,const std::atomic_bool* cancel){
    cancellation=cancel;
    ensure_winsock();addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;hints.ai_flags=AI_NUMERICHOST;addrinfo* found=nullptr;
    if(getaddrinfo(address.c_str(),std::to_string(port).c_str(),&hints,&found)!=0||!found)throw std::runtime_error("netplay: cannot resolve "+address);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(long(timeout_seconds*1000));
    Socket peer=RECVX_INVALID_SOCKET;std::string last="connection refused";
    while(peer==RECVX_INVALID_SOCKET){const Socket s=make_socket();
        set_nonblocking(s);bool connected=::connect(s,found->ai_addr,socklen_t(found->ai_addrlen))==0;
        while(!connected&&!cancelled()&&std::chrono::steady_clock::now()<deadline){fd_set writes;FD_ZERO(&writes);FD_SET(s,&writes);timeval wait{0,20000};const int ready=select(int(s)+1,nullptr,&writes,nullptr,&wait);if(ready<0)break;if(ready>0){int error=0;socklen_t size=sizeof error;connected=getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&size)==0&&error==0;break;}}
        if(connected){peer=s;break;}
        close_socket(s);if((cancelled()||std::chrono::steady_clock::now()>deadline)){freeaddrinfo(found);throw std::runtime_error("netplay: could not connect to "+address+":"+std::to_string(port)+" ("+last+")");}
        std::this_thread::sleep_for(std::chrono::milliseconds(250));}
    freeaddrinfo(found);set_nodelay(peer);set_nonblocking(peer);
    bool host_player2=false;try{host_player2=handshake(peer,session,delay,timeout_seconds,false);}catch(...){close_socket(peer);throw;}
    auto result=std::unique_ptr<Netplay>(new Netplay(Role::Guest,std::intptr_t(peer),delay));result->host_player2_=host_player2;return result;
}
Netplay::Packed Netplay::pack(const InputFrame& frame){Packed p;p.gamepad=frame.gamepad_input?1:0;
    for(unsigned i=0;i<action_count;++i){if(frame.held[i])p.held|=1u<<i;if(frame.pressed[i])p.pressed|=1u<<i;if(frame.repeat[i])p.repeat|=1u<<i;}return p;}
InputFrame Netplay::unpack(const Packed& p){InputFrame f;f.gamepad_input=p.gamepad!=0;
    for(unsigned i=0;i<action_count;++i){f.held[i]=(p.held>>i)&1u;f.pressed[i]=(p.pressed>>i)&1u;f.repeat[i]=(p.repeat>>i)&1u;}return f;}
static_assert(action_count<=32,"netplay packs actions into 32-bit masks");
bool Netplay::send_bytes(const std::vector<std::uint8_t>& bytes){
    if(!connected_)return false;std::size_t done=0;auto spins=0;
    while(done<bytes.size()){const auto n=::send(Socket(socket_),reinterpret_cast<const char*>(bytes.data()+done),int(bytes.size()-done),send_flags);
        if(n>0){done+=std::size_t(n);continue;}
        if(n==RECVX_SOCKET_ERROR&&would_block()){if(++spins>2000){fail("send stalled");return false;}std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
        fail("send failed");return false;}
    return true;
}
void Netplay::fail(const std::string& why){if(connected_){connected_=false;error_=why;}}
void Netplay::submit(unsigned tick,const InputFrame& frame){
    const unsigned target=tick+delay_;if(local_.count(target))return;
    const auto packed=pack(frame);local_[target]=packed;
    std::vector<std::uint8_t> out{Input};put32(out,target);put32(out,packed.held);put32(out,packed.pressed);put32(out,packed.repeat);out.push_back(packed.gamepad);
    if(send_bytes(out))++sent_;
}
void Netplay::hash(unsigned tick,const std::array<std::uint64_t,parts>& value){
    local_hash_[tick]=value;std::vector<std::uint8_t> out{Hash};put32(out,tick);for(const auto v:value)put64(out,v);(void)send_bytes(out);
    if(const auto it=remote_hash_.find(tick);it!=remote_hash_.end()&&it->second!=value&&desync_tick_<0){desync_tick_=long(tick);for(unsigned i=0;i<parts;++i)if(it->second[i]!=value[i])desync_parts_|=1u<<i;}
}
void Netplay::pump(){
    if(!connected_)return;
    for(;;){std::uint8_t buffer[4096];const auto n=::recv(Socket(socket_),reinterpret_cast<char*>(buffer),sizeof buffer,0);
        if(n>0){inbox_.insert(inbox_.end(),buffer,buffer+n);continue;}
        if(n==0){fail("peer closed the connection");break;}
        if(would_block())break;fail("receive failed");break;}
    std::size_t at=0;
    while(at<inbox_.size()){const auto type=inbox_[at];
        if(type==Input){if(inbox_.size()-at<18)break;Packed p;const unsigned tick=get32(&inbox_[at+1]);p.held=get32(&inbox_[at+5]);p.pressed=get32(&inbox_[at+9]);p.repeat=get32(&inbox_[at+13]);p.gamepad=inbox_[at+17];remote_[tick]=p;++received_;at+=18;}
        else if(type==Hash){if(inbox_.size()-at<5+8*parts)break;const unsigned tick=get32(&inbox_[at+1]);std::array<std::uint64_t,parts> value{};for(unsigned i=0;i<parts;++i)value[i]=get64(&inbox_[at+5+8*i]);remote_hash_[tick]=value;
            if(const auto it=local_hash_.find(tick);it!=local_hash_.end()&&it->second!=value&&desync_tick_<0){desync_tick_=long(tick);for(unsigned i=0;i<parts;++i)if(it->second[i]!=value[i])desync_parts_|=1u<<i;}at+=5+8*parts;}
        else if(type==Bye){fail("peer left the session");at+=1;break;}
        else{fail("unknown message from peer");at=inbox_.size();break;}}
    inbox_.erase(inbox_.begin(),inbox_.begin()+std::ptrdiff_t(at));
}
bool Netplay::ready(unsigned tick){pump();return local_.count(tick)&&remote_.count(tick);}
InputFrame Netplay::local(unsigned tick)const{const auto it=local_.find(tick);return it==local_.end()?InputFrame{}:unpack(it->second);}
InputFrame Netplay::remote(unsigned tick)const{const auto it=remote_.find(tick);return it==remote_.end()?InputFrame{}:unpack(it->second);}
void Netplay::forget_before(unsigned tick){
    local_.erase(local_.begin(),local_.lower_bound(tick));remote_.erase(remote_.begin(),remote_.lower_bound(tick));
    local_hash_.erase(local_hash_.begin(),local_hash_.lower_bound(tick));remote_hash_.erase(remote_hash_.begin(),remote_hash_.lower_bound(tick));
}
std::string Netplay::status()const{
    std::string s=role_==Role::Host?"Netplay host":"Netplay guest";
    if(!connected_)return s+": "+error_;
    s+=local_player1()?" / Player 1":" / Player 2";
    s+=" (delay "+std::to_string(delay_)+")";
    if(desync_tick_>=0){s+=" DESYNC at tick "+std::to_string(desync_tick_)+" (";const char* names[parts]={"player 1/room","player 2","enemies","screens/flags"};bool first=true;for(unsigned i=0;i<parts;++i)if(desync_parts_&(1u<<i)){s+=std::string(first?"":", ")+names[i];first=false;}s+=")";}
    return s;
}
}

namespace recvx {
namespace {
const char* const word_a[]={"amber","blue","calm","dark","east","fair","gold","green","high","jade","kind","lone","moon","north","red","wild"};
const char* const word_b[]={"bird","brook","cat","deer","elm","fern","fox","hill","lake","leaf","oak","owl","pine","rain","rock","wolf"};
std::string byte_word(unsigned n){return std::string(word_a[n>>4])+word_b[n&15];}
}
std::pair<std::string,unsigned short> Netplay::endpoint(const std::string& text){
    std::string value=text;std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(value.find('.')==std::string::npos){
        std::replace(value.begin(),value.end(),'-',' ');std::istringstream in(value);std::string word;std::vector<unsigned> bytes;
        while(in>>word){unsigned n=0;for(;n<256&&byte_word(n)!=word;++n){}if(n==256||bytes.size()==7)throw std::runtime_error("Invalid address phrase");bytes.push_back(n);}
        if(bytes.size()!=7)throw std::runtime_error("Address phrase needs seven words");
        unsigned check=0x5a;for(unsigned i=0;i<6;++i)check=(check*33+bytes[i])&255;
        if(check!=bytes[6])throw std::runtime_error("Address phrase checksum differs");
        value=std::to_string(bytes[0])+"."+std::to_string(bytes[1])+"."+std::to_string(bytes[2])+"."+std::to_string(bytes[3])+":"+std::to_string(bytes[4]*256+bytes[5]);
    }
    unsigned port=default_port;const auto colon=value.find(':');if(colon!=std::string::npos){const auto number=value.substr(colon+1);if(number.empty()||number.size()>5||number.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid port");port=unsigned(std::stoul(number));value.erase(colon);}
    in_addr ip{};if(!port||port>65535||inet_pton(AF_INET,value.c_str(),&ip)!=1)throw std::runtime_error("Use an IPv4 address and port");
    return {value,static_cast<unsigned short>(port)};
}
std::string Netplay::phrase(const std::string& text){const auto e=endpoint(text);in_addr ip{};inet_pton(AF_INET,e.first.c_str(),&ip);const unsigned n=ntohl(ip.s_addr);const unsigned bytes[]={n>>24,(n>>16)&255,(n>>8)&255,n&255,unsigned(e.second)>>8,unsigned(e.second)&255};std::string out;unsigned check=0x5a;for(auto b:bytes){if(!out.empty())out+=' ';out+=byte_word(b);check=(check*33+b)&255;}return out+' '+byte_word(check);}
std::string Netplay::local_address(){ensure_winsock();char name[256]{};if(gethostname(name,sizeof(name)-1)!=0)return "127.0.0.1";addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;addrinfo* addresses=nullptr;std::string out="127.0.0.1";if(getaddrinfo(name,nullptr,&hints,&addresses)==0){for(auto p=addresses;p;p=p->ai_next){const auto* a=reinterpret_cast<const sockaddr_in*>(p->ai_addr);char buffer[INET_ADDRSTRLEN]{};if(inet_ntop(AF_INET,&a->sin_addr,buffer,sizeof buffer)&&std::string(buffer).rfind("127.",0)!=0){out=buffer;break;}}freeaddrinfo(addresses);}return out;}
}
