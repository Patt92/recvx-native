#include "audio_pitch.h"
#include <algorithm>
#include <stdexcept>
namespace recvx {
namespace {
#include "audio_pitch_tables.inc"
}
std::uint16_t sony_note_pitch(std::uint16_t root,std::uint16_t root_fine,std::uint16_t note,std::int16_t fine){
    const int total=int(fine)+int(root_fine),quotient=total/128;
    const int delta=std::int16_t(int(note)+quotient-int(root));
    const int octave=delta/12;
    int semitone=delta-12*octave,residual=total-128*quotient,shift=octave-2;
    if(semitone<0||(semitone==0&&residual<0)){semitone+=12;shift=octave-3;}
    // LIBSD 0x3440 retains its quotient-dependent adjustment; callers normally normalize negative fine first.
    if(residual<0){semitone=semitone-1+quotient;residual+=(quotient+1)*128;}
    if(semitone<0||semitone>=12||residual<0||residual>=128)throw std::runtime_error("Sony note pitch outside the original lookup tables");
    const std::uint32_t product=std::uint32_t(semitones[semitone])*fractions[residual];
    std::uint32_t value=std::uint32_t(std::int32_t(product)>>16);
    if(shift<0){const unsigned count=unsigned(-shift)&31u;value=(value+(1u<<((count-1)&31u)))>>count;}
    return std::uint16_t(value);
}
std::uint16_t sony_sample_pitch(unsigned rate,std::uint16_t root,int note,int fine){
    if(fine<0){const int borrow=(-fine+127)/128;fine+=borrow*128;note-=borrow;}
    const auto pitch=sony_note_pitch(root,0,std::uint16_t(note),std::int16_t(fine));
    const auto scale=std::uint32_t(std::uint64_t(rate)*65536u/48000u);
    return std::uint16_t(std::min<std::uint32_t>((std::uint32_t(pitch)*scale)>>16,0x3fff));
}
}
