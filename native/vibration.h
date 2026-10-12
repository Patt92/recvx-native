#pragma once
#include <array>
namespace recvx {
// Controller vibration as the original drives it: sdfunc.c StartVibrationEx
// -> StartVibrationBasic (VibP[32]/VibFlag[5], the sys->ss_flg 0x400000 option
// and SetEventVibrationMode gate) -> vibman.c StartVibration -> ps2_sg_pdvib.c
// pdVibMxStart (20 PAD_ACT envelopes) and Ps2_pad_actuater, which runs once per
// VSync and hands the big-motor level to scePadSetActDirect. The host reads the
// resulting level once per 30 Hz game tick (two VSyncs per Ps2SwapDBuff).
class VibrationUnit {
public:
    void set_enabled(bool on){enabled_=on;}            // SetUseVibrationUnit
    bool enabled()const{return enabled_;}
    void set_option_off(bool off){option_off_=off;}    // sys->ss_flg & 0x400000
    void set_event_mode(int mode){event_mode_=mode;}   // SetEventVibrationMode
    int event_mode()const{return event_mode_;}
    void set_unit_present(bool present){present_=present;} // pdVibMxIsReady
    // StartVibrationEx(AtrbId, VibNo). Returns the vibman.c result (0 = started
    // or vibration disabled, -1 = no free envelope, -2 = no unit).
    int start(int atrb_id,int vib_no);
    void stop();                                       // StopVibrationEx
    // Two Ps2_pad_actuater passes (one game tick); returns the big-motor level 0..255.
    unsigned tick();
    unsigned level()const{return motor_;}
    unsigned starts()const{return starts_;}
    unsigned requests()const{return requests_;} // StartVibrationBasic calls that passed the option/event gate
private:
    struct Act {int be_flag=0;int level=0,start=0,time=0,delay=0;};
    unsigned actuate();
    std::array<Act,20> acts_{};
    bool enabled_=true,option_off_=false,present_=false;
    int event_mode_=0;unsigned motor_=0,starts_=0,requests_=0;
};
}
