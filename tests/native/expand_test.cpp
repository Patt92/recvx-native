#include "expand.h"
#include <iostream>
#include <stdexcept>
#include <string>
using Bytes = std::vector<std::uint8_t>;
static void check(bool value) { if (!value) throw std::runtime_error("test failed"); }
static void rejects(const Bytes& source, std::size_t limit) {
    try { recvx::expand(source, limit); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("malformed input accepted");
}
int main() {
    try {
        check(recvx::expand({2, 0, 0}, 0).empty());
        check(recvx::expand({0x17, 'A', 'B', 'C', 0, 0}, 3) == Bytes({'A','B','C'}));
        check(recvx::expand({0x41, 'A', 0xff, 0, 0}, 3) == Bytes({'A','A','A'}));
        check(recvx::expand({0x15, 'A', 0xf9, 0xff, 0, 0}, 4) == Bytes({'A','A','A','A'}));
        check(recvx::expand({0x15, 'A', 0xf8, 0xff, 3, 0, 0}, 5) == Bytes({'A','A','A','A','A'}));
        // Exercise descriptor refresh twice, with interleaved literal payload.
        Bytes crossing{0xff,'1','2','3','4','5','6','7','8',0xff,'a','b','c','d','e','f','g','h',2,0,0};
        Bytes expected{'1','2','3','4','5','6','7','8','a','b','c','d','e','f','g','h'};
        check(recvx::expand(crossing, 16) == expected);
        rejects({}, 10); rejects({1}, 10); rejects({0, 255}, 10);
        rejects({0x17,'A','B','C',0,0}, 2);
        rejects({0x41,'A',255,0,0}, 2);
        rejects({2,1,0}, 100); rejects({2,0}, 100);
        std::cout << "Expand tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
