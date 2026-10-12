#include "audio_pan.h"
#include <algorithm>
#include <cstdlib>
namespace recvx {
SonyPan sony_pan_factors(int pan){
    SonyPan result;
    if(pan>=127)result.left=0;
    else if(pan>64)result.left=std::uint8_t((127-pan)*128/63);
    else if(pan<=1)result.right=0;
    else if(pan<64)result.right=std::uint8_t((pan-1)*128/63);
    return result;
}
int sony_bank_pan(int program,int split,int sample,int key_pan){
    return std::clamp(std::abs(program)+std::abs(split)+std::abs(sample)-128+key_pan,0,127);
}
}
