#include "vibration.h"
namespace recvx {
namespace {
// sdfunc.c:116 PDS_VIBPARAM_EX VibP[32] = {flag, power, freq, inc}.
struct VibParam {unsigned char flag,power,freq,inc;};
constexpr VibParam VibP[32]={
    {0x01,0x07,0x3B,0x04},{0x02,0x10,0x2C,0x04},{0x01,0x55,0x1D,0x04},{0x00,0x01,0x21,0x01},
    {0x01,0xF9,0x0F,0x02},{0x01,0xF9,0x0F,0x03},{0x00,0x55,0x0F,0x04},{0x02,0xFF,0x19,0x05},
    {0x02,0xFE,0x2D,0x05},{0x02,0x01,0x14,0x08},{0x01,0x04,0x1E,0x01},{0x02,0x07,0x3B,0x02},
    {0x01,0xFE,0x0F,0x05},{0x02,0x07,0x1D,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},
    {0x00,0x50,0x0F,0x02},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},
    {0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},
    {0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},
    {0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01},{0x00,0x00,0x0F,0x01}};
// sdfunc.c:148 char VibFlag[5].
constexpr unsigned char VibFlag[5]={0x01,0x08,0x80,0x09,0x81};
}
int VibrationUnit::start(int atrb_id,int vib_no){
    // sdfunc.c:3382 StartVibrationBasic: option bit and event mode gate.
    if(option_off_||!(event_mode_==0||atrb_id==2))return 0;
    if(vib_no<0||vib_no>=32)return 0; // VibP[32]; the original reads past the table here
    const auto& p=VibP[vib_no];
    const unsigned flag=p.flag<5?VibFlag[p.flag]:0u;
    ++requests_;
    // vibman.c:58 StartVibration: EnadleVibrationFlag, port 2 of the pad, unit ready.
    if(!enabled_)return 0;
    if(!present_)return -2;
    // ps2_sg_pdvib.c:46 pdVibMxStart: first free PAD_ACT of twenty.
    unsigned cnt=0;
    for(;cnt<acts_.size();++cnt)if(acts_[cnt].be_flag==0)break;
    if(cnt>=acts_.size())return -1;
    auto& a=acts_[cnt];
    a.be_flag=3;
    a.level=a.start=p.power>85?255:int(p.power)+170;
    a.time=flag==1?9:flag==8?6:4;
    a.delay=0;
    ++starts_;
    return 0;
}
void VibrationUnit::stop(){
    // vibman.c:82 StopVibration -> pdVibMxStop -> Ps2_pad_act_all_stop.
    if(!enabled_||!present_)return;
    for(auto& a:acts_)a.be_flag=0;
    motor_=0;
}
unsigned VibrationUnit::actuate(){
    // ps2_sg_pdvib.c:113 Ps2_pad_actuater: nineteen entries are serviced; the
    // twentieth is allocated by pdVibMxStart but never stepped (original loop bound).
    unsigned motor=0;
    for(unsigned cnt=0;cnt<19;++cnt){
        auto& a=acts_[cnt];
        if(!(a.be_flag&3))continue;
        if(a.delay>0){--a.delay;continue;}
        if(a.time>0){
            // Ps2_pad_act_start: f_add/f_level stay zero for pdVibMxStart envelopes,
            // so the level is the start level for the whole time.
            if(a.be_flag&2){const unsigned level=unsigned(a.level)&0xFFu;if(motor<level)motor=level;}
            --a.time;
        }else a.be_flag=0; // Ps2_pad_act_stop
    }
    return motor;
}
unsigned VibrationUnit::tick(){
    const unsigned first=actuate();const unsigned second=actuate();
    motor_=first>second?first:second;
    return motor_;
}
}
