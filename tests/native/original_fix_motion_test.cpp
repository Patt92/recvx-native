/* ORIGINAL-FIX (QUALITY.md) in native/motion_set.cpp, Motion.c:119-190.
   Prueft je Fix den Defektfall und dass der Normalfall unveraendert bleibt.
   Die Clips sind Testdaten in der Form von MotionClip, keine Originalmotionen. */
#include "motion_set.h"
#include "actor.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace recvx;
static void check(bool yes,const char* reason){if(!yes)throw std::runtime_error(reason);}

// Clip mit `channels` Kanaelen und `frames` Bildern; Kanal c in Bild f liegt bei x = 100*c + f.
// Flag 0x10 waehlt SetMtnNormal (Motion.c:131), das jeden Kanal in sein Objekt schreibt.
static MotionClip clip(unsigned channels,unsigned frames){
    MotionClip c;c.frames=frames;c.flags=0x10;
    c.positions.assign(channels,{});c.angles.assign(channels,{});
    for(unsigned ch=0;ch<channels;++ch)for(unsigned f=0;f<frames;++f){
        c.positions[ch].push_back({float(100*ch+f),0,0});
        c.angles[ch].push_back({std::int32_t(ch),std::int32_t(f),0});}
    return c;}

int main(){try{
    {
        // Motion mit 3 Kanaelen auf einem Modell mit 2 Objekten (RM_3210/7060 id 88: 10 auf 9).
        // Original schreibt objP[2] hinter das Feld; Regel 2: Kanal verbraucht, Schreibzugriff entfaellt.
        const auto c=clip(3,2);std::vector<ModelPose> obj(2);const std::vector<std::uint32_t> owp(2,0);
        MotionWork m;m.frm_no=1<<16;std::string gap;
        set_motion(m,0,0,&c,nullptr,obj,owp,&gap);
        check(gap.empty(),"ueberzaehliger Kanal meldet noch eine Luecke");
        check(obj[0].position.x==1.0f&&obj[1].position.x==101.0f,"Kanaele 0/1 nicht in Objekt 0/1 (Normalfall)");
    }
    {
        // Bild 9226 auf einem 2-Bild-Clip (RM_3120, event.c:11897). Regel 3: naechstes gueltiges Bild = 1.
        const auto c=clip(2,2);std::vector<ModelPose> obj(2);const std::vector<std::uint32_t> owp(2,0);
        MotionWork m;m.frm_no=9226<<16;std::string gap;
        set_motion(m,0,0,&c,nullptr,obj,owp,&gap);
        check(gap.empty(),"Bild hinter dem Clip meldet noch eine Luecke");
        check(obj[0].position.x==1.0f&&obj[1].position.x==101.0f,"Bild hinter dem Clip nicht auf das letzte Bild geklemmt");
        // Die Bilduhr folgt weiter Motion.c:106-108: frm_no >= frm_num zieht frm_num einmal ab.
        check(m.frm_no==((9226-2)<<16),"Fix veraendert die Bilduhr frm_no");
    }
    {
        // Mit angehaengten Nachbarkeys (clip.tail_keys, Folgeblockbytes): innerhalb bleibt der
        // Originallesezugriff, weit dahinter klemmt Regel 3 auf das letzte Clipbild, nicht auf den
        // letzten Nachbarkey (RM_3120: dort -2.69e38, auf dem Host inf).
        auto c=clip(2,2);c.tail_keys=1;
        for(unsigned ch=0;ch<2;++ch){c.positions[ch].push_back({-2.69e38f,0,0});c.angles[ch].push_back({0,0,0});}
        std::vector<ModelPose> obj(2);const std::vector<std::uint32_t> owp(2,0);
        MotionWork m;m.frm_no=9226<<16;std::string gap;
        set_motion(m,0,0,&c,nullptr,obj,owp,&gap);
        check(gap.empty()&&obj[0].position.x==1.0f&&obj[1].position.x==101.0f,"weit hinter dem Clip auf einen Nachbarkey statt auf das letzte Clipbild geklemmt");
        MotionWork tail;tail.frm_no=2<<16;
        set_motion(tail,0,0,&c,nullptr,obj,owp,&gap);
        check(obj[0].position.x==-2.69e38f,"Nachbarkey direkt hinter dem Clip nicht mehr gelesen (Motion.c:131)");
    }
    {
        // Normalfall: Bild 0 im Bereich bleibt Bild 0.
        const auto c=clip(2,2);std::vector<ModelPose> obj(2);const std::vector<std::uint32_t> owp(2,0);
        MotionWork m;m.frm_no=0;std::string gap;
        set_motion(m,0,0,&c,nullptr,obj,owp,&gap);
        check(gap.empty()&&obj[0].position.x==0.0f&&obj[1].position.x==100.0f,"Bild 0 im Bereich veraendert");
    }
    std::cout<<"ORIGINAL-FIX motion: surplus channel, out-of-range frame and in-range frame passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"original fix motion test failed: "<<e.what()<<'\n';return 1;}}
