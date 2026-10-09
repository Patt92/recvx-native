#include "status_pulse.h"
#include <algorithm>
namespace recvx {
// EKG des Statusbildschirms, eigenständig nativ (D08). Semantikreferenz
// sub1.c PulseInit, PulseMain, PulseAnim, PulseFadeAnim, PulseHealAnim,
// PulsePoisonHealAnim, Pulse00; kein Funktionskörper übernommen.
namespace {
// PulseInit panelpos: Endpunkte der fünf Kurvensegmente je Zustand.
constexpr float kPanel[5][5]={
    {34,89,144,216,0},{34,89,144,216,0},{23,82,141,216,0},{34,74,118,216,0},{23,113,183,216,0}};
// PulseInit ocol01 je Zustand (ocol00 ist 0, außer Zustand 0 Segmente 1..3).
constexpr std::uint32_t kTrailColour[5]={0xff80ff80u,0x00adad00u,0x00d64200u,0x00ad0000u,0x00bd008cu};
}
void StatusPulse::init(){
    // PulseInit: alles zurück, Segmentziele und Farben je st->hp.
    const unsigned state=hp_<5?hp_:0;
    for(unsigned i=0;i<5;++i){
        work_.pos_00[i]={0,0};work_.pos_01[i]={0,0};
        work_.col00[i]=0xffffffffu;work_.col01[i]=0xffffffffu;
        work_.uv_00[i]=0;work_.uv_01[i]=0;
        work_.ocol00[i]=(state==0&&i!=0&&i!=4)?0xff00ff00u:0u;
        work_.ocol01[i]=kTrailColour[state];
        work_.pos_02[i]={kPanel[state][i],0};
    }
    work_.pulseflg=0x1;work_.fadeflg=0x1;work_.count=0;
    work_.heal_pos00={0,0};work_.heal_pos01={0,0};
}
void StatusPulse::anim(){
    // PulseAnim: Segment n läuft mit 4 px je Frame bis pos_02[n]; am Ziel
    // übernimmt Segment n+1 den Startpunkt, das nächste Bit wird gesetzt und
    // der Fade des Segments beginnt.
    for(unsigned seg=0;seg<4;++seg){
        const unsigned bit=1u<<seg;
        if(!(work_.pulseflg&bit))continue;
        auto& head=work_.pos_01[seg];const float goal=work_.pos_02[seg].x;
        bool reached=false;
        if(head.x<goal){
            head.x+=4.0f;work_.uv_01[seg]=std::int16_t(work_.uv_01[seg]+4);
            reached=head.x>goal-1.0f;
        }else reached=head.x>goal-1.0f;
        if(reached){
            head.x=goal;work_.uv_01[seg]=std::int16_t(goal);
            work_.pos_00[seg+1].x=work_.pos_01[seg+1].x=head.x;
            work_.uv_00[seg+1]=work_.uv_01[seg+1]=work_.uv_01[seg];
            work_.pulseflg=(work_.pulseflg|(bit<<1))&~bit;
            work_.fadeflg|=bit<<1;
        }
    }
    fade();
    if(work_.pulseflg&0x20)init();
}
void StatusPulse::fade(){
    // PulseFadeAnim: Alpha der Segmente sinkt um 8 je Frame, gesteuert über
    // fadeflg; erreicht col01[3] Alpha 0, startet die Kurve neu (pulseflg 0x20).
    // RGB der Offsetfarben bleibt unverändert (die Original-Begrenzung auf <0
    // greift für Bytewerte nie).
    std::array<int,5> a00{},a01{};
    for(unsigned i=0;i<5;++i){a00[i]=int(work_.col00[i]>>24);a01[i]=int(work_.col01[i]>>24);}
    const unsigned f=work_.fadeflg;
    if((f&0x1)&&a00[0]>0)a00[0]-=8;
    if(f&0x2){if(a01[0]>0)a01[0]-=8;if(a00[1]>0)a00[1]-=8;}
    if(f&0x4){if(a01[1]>0)a01[1]-=8;if(a00[2]>0)a00[2]-=8;}
    if(f&0x8){if(a01[2]>0)a01[2]-=8;if(a00[3]>0)a00[3]-=8;}
    if((f&0x10)&&a01[3]>0)a01[3]-=8;
    for(unsigned i=0;i<4;++i){
        a00[i]=std::max(a00[i],0);a01[i]=std::max(a01[i],0);
        if(a01[3]==0)work_.pulseflg|=0x20;
        work_.col00[i]=(work_.col00[i]&0xffffffu)|(std::uint32_t(a00[i])<<24);
        work_.col01[i]=(work_.col01[i]&0xffffffu)|(std::uint32_t(a01[i])<<24);
    }
}
void StatusPulse::trace(float x,float y,int u,int v){
    // Pulse00: fünf Streifen pos_00..pos_01 je Segment, 72 px hoch, z 0.1;
    // UV aus der Zustandszeile des Atlas plus Segmentversatz.
    for(unsigned i=0;i<5;++i){
        const float x0=x+work_.pos_00[i].x,x1=x+work_.pos_01[i].x;
        const float u0=float(u+work_.uv_00[i])/512.0f,u1=float(u+work_.uv_01[i])/512.0f;
        const float v0=float(v)/512.0f,v1=float(v+72)/512.0f;
        PulseQuad q;
        q[0]={x0,y,u0,v0,work_.col00[i],work_.ocol00[i],0.1f};
        q[1]={x0,72.0f+y,u0,v1,work_.col00[i],work_.ocol00[i],0.1f};
        q[2]={x1,y,u1,v0,work_.col01[i],work_.ocol01[i],0.1f};
        q[3]={x1,72.0f+y,u1,v1,work_.col01[i],work_.ocol01[i],0.1f};
        quads_.push_back(q);
    }
}
void StatusPulse::heal(float x,float y){
    // PulseHealAnim: grünes Band wandert 16 px je Frame über 216 px; der
    // Schwanz folgt, sobald der Kopf 54 px erreicht. Ende löscht statusflg 0x20.
    if(work_.pulseflg&0x1){
        if(work_.heal_pos01.x<216.0f){work_.heal_pos01.x+=16.0f;if(work_.heal_pos01.x>54.0f)work_.pulseflg|=0x2;}
        else work_.pulseflg&=~0x1u;
    }
    if(work_.pulseflg&0x2){
        if(work_.heal_pos00.x<216.0f)work_.heal_pos00.x+=16.0f;
        else{statusflg_&=~0x20u;work_.pulseflg&=~0x42u;}
    }
    PulseQuad q;
    q[0]={x+work_.heal_pos00.x,y,0.84375f,0.8125f,0,0,0.2f};
    q[1]={x+work_.heal_pos00.x,72.0f+y,0.84375f,0.8144531f,0,0,0.2f};
    q[2]={x+work_.heal_pos01.x,y,0.8457031f,0.8125f,0xff00ff00u,0xff00ff00u,0.2f};
    q[3]={x+work_.heal_pos01.x,72.0f+y,0.8457031f,0.8144531f,0xff00ff00u,0xff00ff00u,0.2f};
    quads_.push_back(q);
}
void StatusPulse::poison_heal(float x,float y){
    // PulsePoisonHealAnim: blaues Band steigt 16 px je Frame über 72 px; der
    // Fuß folgt ab 32 px. Ende löscht statusflg 0x20.
    if(work_.pulseflg&0x1){
        if(work_.heal_pos01.y<72.0f){work_.heal_pos01.y+=16.0f;if(work_.heal_pos01.y>32.0f)work_.pulseflg|=0x2;}
        else work_.pulseflg&=~0x1u;
    }
    if(work_.pulseflg&0x2){
        if(work_.heal_pos00.y<72.0f)work_.heal_pos00.y+=16.0f;
        else{statusflg_&=~0x20u;work_.pulseflg&=~0x82u;}
    }
    PulseQuad q;
    q[0]={x,y-work_.heal_pos01.y,0.5644531f,0.3125f,0xff0000ffu,0x000000ffu,0.2f};
    q[1]={x,y-work_.heal_pos00.y,0.5644531f,0.31640625f,0x800000ffu,0,0.2f};
    q[2]={216.0f+x,y-work_.heal_pos01.y,0.57421875f,0.3125f,0xff0000ffu,0x000000ffu,0.2f};
    q[3]={216.0f+x,y-work_.heal_pos00.y,0.57421875f,0.31640625f,0x800000ffu,0,0.2f};
    quads_.push_back(q);
}
void StatusPulse::open(unsigned condition){hp_=condition;statusflg_=0;opened_=true;quads_.clear();init();}
void StatusPulse::begin_cure(unsigned condition,unsigned pulseflg){
    // Use_00: st->hp = abc, PulseInit, pulseflg |= 0x40/0x80, statusflg |= 0x20.
    if(!opened_){hp_=condition;opened_=true;quads_.clear();}
    hp_=condition;init();
    work_.pulseflg|=pulseflg&0xc0u;
    statusflg_|=0x20;
}
unsigned StatusPulse::heal_mode()const{
    if(!healing())return 0;
    return (work_.pulseflg&0x40)?1u:(work_.pulseflg&0x80)?2u:0u;
}
void StatusPulse::tick(float center_x,float center_y){
    // PulseMain: ohne statusflg 0x20 Kurve in der Zustandszeile; sonst
    // Heil- bzw. Gegengiftband und nach dessen Ende PulseInit.
    if(!opened_)return;
    quads_.clear();
    if(!(statusflg_&0x20)){
        static constexpr int u[5]={0,0,0,216,216},v[5]={296,368,440,368,440};
        const unsigned state=hp_<5?hp_:0;
        anim();
        trace(176.0f+center_x,134.0f+center_y,u[state],v[state]);
    }else{
        if(work_.pulseflg&0x40)heal(176.0f+center_x,134.0f+center_y);
        else if(work_.pulseflg&0x80)poison_heal(176.0f+center_x,206.0f+center_y);
        if(!(statusflg_&0x20))init();
    }
}
}
