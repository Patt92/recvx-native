#include "motion_set.h"
#include "ninja_math.h"
#include <cmath>

// Native determination of Motion.c bhSetMotion and its six SetMtn* variants
// (semantics reference only; no original body is compiled or linked). The
// object tree is the native work's own pose store, the motion keys are the
// decoded MTN of the existing asset owner.
namespace recvx {
namespace {
// Motion.c:5 mka_ang is file static and shared by every hokan call. Without
// angle keys SetMtnSlowHokan writes only [0] (Motion.c:798), so [1]/[2] keep
// the previous call's values; that carry-over is original behaviour.
std::int32_t mka_ang[3];

// Motion.c:969 AngZyxToYzx / :991 AngYzxToZyx.
void ang_zyx_to_yzx(const std::int32_t* zyx,std::int32_t* yzx){
    float s0,s1,s2,c0,c1,c2;
    njSinCos(zyx[0],&s0,&c0);njSinCos(zyx[1],&s1,&c1);njSinCos(zyx[2],&s2,&c2);
    const float b=s2*s1;
    yzx[0]=std::int32_t(10430.381f*std::atan2(c2*s0-b*c0,c2*c0+b*s0));
    yzx[1]=std::int32_t(10430.381f*std::atan2(s1,c2*c1));
    yzx[2]=std::int32_t(10430.381f*std::asin(s2*c1));
}
void ang_yzx_to_zyx(const std::int32_t* yzx,std::int32_t* zyx){
    float s0,s1,s2,c0,c1,c2;
    njSinCos(yzx[0],&s0,&c0);njSinCos(yzx[1],&s1,&c1);njSinCos(yzx[2],&s2,&c2);
    const float b=s1*s2;
    zyx[0]=std::int32_t(10430.381f*std::atan2(c1*s0+b*c0,c1*c0-b*s0));
    zyx[1]=std::int32_t(10430.381f*std::asin(s1*c2));
    zyx[2]=std::int32_t(10430.381f*std::atan2(s2,c1*c2));
}
inline std::int16_t s16(std::int32_t v){return std::int16_t(std::uint16_t(std::uint32_t(v)));}

// The hokan pre-step shared by every variant (Motion.c:268-310): choose the
// nearer of the two equivalent Euler triples before blending.
void flip_nearer(std::int32_t* ang,const std::int32_t* dst){
    std::int16_t dlt=s16(dst[0]-ang[0]);
    std::int32_t sum=dlt<0?-(dlt+s16(dlt+32768)):dlt+s16(dlt+32768);
    dlt=s16(dst[1]-ang[1]);
    if(dlt<0)sum-=dlt;else sum+=dlt;
    dlt=s16(dst[1]-(32768-ang[1]));
    if(dlt<0)sum+=dlt;else sum-=dlt;
    dlt=s16(dst[2]-ang[2]);
    if(dlt<0)sum-=dlt+s16(dlt+32768);else sum+=dlt+s16(dlt+32768);
    if(sum>0){ang[0]=32768+ang[0];ang[1]=32768-ang[1];ang[2]=32768+ang[2];}
}
void blend(std::int32_t* ang,float rate){
    for(unsigned k=0;k<3;++k)ang[k]+=std::int32_t(rate*float(s16(mka_ang[k]-ang[k])));
}

struct Pass {
    const MotionClip& clip;const std::int8_t* datP;std::vector<ModelPose>& obj;const std::vector<std::uint32_t>& owp;std::string* gap;
    bool fail(const char* what){if(gap&&gap->empty())*gap=what;return false;}
    // NormalTbl (Motion.c:7) is the identity over 64 nodes.
    bool node(std::size_t at,std::size_t& out){
        if(!datP&&at>=64)return fail("bhSetMotion:NormalTbl[64] ueberschritten");
        const std::int32_t n=datP?datP[at]:std::int32_t(at);
        if(n<0||std::size_t(n)>=obj.size()||std::size_t(n)>=owp.size())return fail("bhSetMotion:objP/owP-Index ausserhalb des Modells");
        out=std::size_t(n);return true;}
    bool channel(std::size_t c){return c<clip.positions.size()&&c<clip.angles.size()?true:fail("bhSetMotion:md2P ausserhalb der Motion");}
    bool frame(std::int32_t f){return f>=0&&std::uint32_t(f)<clip.frames?true:fail("bhSetMotion:frm_no ausserhalb von frm_num");}
};

// Motion.c:131 SetMtnNormal: an owP-0x4 node advances datP only.
bool normal(Pass& p,std::int32_t frm_no,unsigned mode){
    const std::int32_t f=frm_no/65536;const float flp_f=(mode&2)?-1.f:1.f;const std::int32_t flp_s=(mode&2)?-1:1;
    std::size_t at=0,ch=0;
    for(std::int32_t left=std::int32_t(p.clip.positions.size());left>0;++at){
        std::size_t n;if(!p.node(at,n))return false;
        if(p.owp[n]&4)continue;
        if(!p.channel(ch)||!p.frame(f))return false;
        auto& o=p.obj[n];
        if(!p.clip.positions[ch].empty()){const auto& k=p.clip.positions[ch][std::size_t(f)];o.position={k.x*flp_f,k.y,k.z};}
        if(!(p.owp[n]&2)){if(!p.clip.angles[ch].empty()){const auto& k=p.clip.angles[ch][std::size_t(f)];o.angles={k[0],k[1]*flp_s,k[2]*flp_s};}
            else o.angles[0]=0;} // Motion.c:184 assigns dstP[0] three times
        ++ch;--left;
    }
    return true;
}
// Motion.c:194 SetMtnNormalHokan: every node consumes a channel (for header).
bool normal_hokan(Pass& p,std::int32_t frm_no,unsigned mode,float rate){
    const std::int32_t f=frm_no/65536;const float flp_f=(mode&2)?-1.f:1.f;const std::int32_t flp_s=(mode&2)?-1:1;
    std::size_t at=0;
    for(std::size_t ch=0;ch<p.clip.positions.size();++ch,++at){
        std::size_t n;if(!p.node(at,n))return false;
        if(p.owp[n]&4)continue;
        if(!p.channel(ch)||!p.frame(f))return false;
        auto& o=p.obj[n];
        if(!p.clip.positions[ch].empty()){const auto& k=p.clip.positions[ch][std::size_t(f)];
            if(mode&0x100)o.position={k.x*flp_f,k.y,k.z};
            else {o.position.x+=rate*(k.x*flp_f-o.position.x);o.position.y+=rate*(k.y-o.position.y);o.position.z+=rate*(k.z-o.position.z);}}
        if(!(p.owp[n]&2)){
            if(!p.clip.angles[ch].empty()){const auto& k=p.clip.angles[ch][std::size_t(f)];mka_ang[0]=k[0];mka_ang[1]=k[1]*flp_s;mka_ang[2]=k[2]*flp_s;}
            else mka_ang[0]=mka_ang[1]=mka_ang[2]=0; // NullMka
            flip_nearer(o.angles.data(),mka_ang);blend(o.angles.data(),rate);
        }
    }
    return true;
}
// Motion.c:337 SetMtnFast: the root position comes from channel 0 only.
bool fast(Pass& p,std::int32_t frm_no,unsigned mode){
    const std::int32_t f=frm_no/65536;const std::int32_t flp=(mode&2)?-1:1;
    std::size_t root;if(!p.node(0,root)||!p.channel(0)||!p.frame(f))return false;
    if(p.clip.positions[0].empty())return p.fail("bhSetMotion:SetMtnFast ohne Wurzelschluessel");
    {const auto& k=p.clip.positions[0][std::size_t(f)];p.obj[root].position={k.x*((mode&2)?-1.f:1.f),k.y,k.z};}
    std::size_t at=0,ch=0;
    for(std::int32_t left=std::int32_t(p.clip.positions.size());left>0;++at){
        std::size_t n;if(!p.node(at,n))return false;
        if(p.owp[n]&4)continue;
        if(!p.channel(ch))return false;
        if(!(p.owp[n]&2)){auto& a=p.obj[n].angles;if(!p.clip.angles[ch].empty()){const auto& k=p.clip.angles[ch][std::size_t(f)];a={k[0],k[1]*flp,k[2]*flp};}else a={0,0,0};}
        ++ch;--left;
    }
    return true;
}
// Motion.c:400 SetMtnFastHokan.
bool fast_hokan(Pass& p,std::int32_t frm_no,unsigned mode,float rate){
    const std::int32_t f=frm_no/65536;const float flp_f=(mode&2)?-1.f:1.f;const std::int32_t flp=(mode&2)?-1:1;
    std::size_t root;if(!p.node(0,root)||!p.channel(0)||!p.frame(f))return false;
    if(p.clip.positions[0].empty())return p.fail("bhSetMotion:SetMtnFastHokan ohne Wurzelschluessel");
    {const auto& k=p.clip.positions[0][std::size_t(f)];auto& pos=p.obj[root].position;
        if(mode&0x100)pos={k.x*flp_f,k.y,k.z};
        else {pos.x+=rate*(k.x*flp_f-pos.x);pos.y+=rate*(k.y-pos.y);pos.z+=rate*(k.z-pos.z);}}
    std::size_t at=0,ch=0;
    for(std::int32_t left=std::int32_t(p.clip.positions.size());left>0;++at){
        std::size_t n;if(!p.node(at,n))return false;
        if(p.owp[n]&4)continue;
        if(!p.channel(ch))return false;
        if(!(p.owp[n]&2)){auto* ang=p.obj[n].angles.data();
            if(!p.clip.angles[ch].empty()){const auto& k=p.clip.angles[ch][std::size_t(f)];mka_ang[0]=k[0];mka_ang[1]=k[1]*flp;mka_ang[2]=k[2]*flp;}
            else mka_ang[0]=mka_ang[1]=mka_ang[2]=0;
            flip_nearer(ang,mka_ang);
            if(!(p.owp[n]&8))blend(ang,rate);
            else {std::int32_t s[3],d[3];ang_zyx_to_yzx(ang,s);ang_zyx_to_yzx(mka_ang,d);
                for(unsigned k=0;k<3;++k)s[k]+=std::int32_t(rate*float(s16(d[k]-s[k])));ang_yzx_to_zyx(s,ang);}
        }
        ++ch;--left;
    }
    return true;
}
// Motion.c:583 SetMtnSlow / :676 SetMtnSlowHokan: keys blended by the
// fraction of frm_no; the following key wraps to 0 at frm_num.
bool slow(Pass& p,std::int32_t frm_no,unsigned mode,bool hokan,float rate){
    const float frac=0.000015258789f*float(frm_no&0xffff);const std::int32_t src=frm_no/65536;std::int32_t dst=src+1;if(std::uint32_t(dst)>=p.clip.frames)dst=0;
    const float flp_f=(mode&2)?-1.f:1.f;const std::int32_t flp_s=(mode&2)?-1:1;
    std::size_t at=0,ch=0;
    for(std::int32_t left=std::int32_t(p.clip.positions.size());left>0;++at){
        std::size_t n;if(!p.node(at,n))return false;
        if(p.owp[n]&4)continue;
        if(!p.channel(ch)||!p.frame(src))return false;
        auto& o=p.obj[n];
        if(!p.clip.positions[ch].empty()){const auto& a=p.clip.positions[ch][std::size_t(src)];const auto& b=p.clip.positions[ch][std::size_t(dst)];
            const NJS_POINT3 k{flp_f*(a.x+frac*(b.x-a.x)),a.y+frac*(b.y-a.y),a.z+frac*(b.z-a.z)};
            if(!hokan||(mode&0x100))o.position=k;
            else {o.position.x+=rate*(k.x-o.position.x);o.position.y+=rate*(k.y-o.position.y);o.position.z+=rate*(k.z-o.position.z);}}
        if(!(p.owp[n]&2)){
            // The keys are read as signed shorts here (Motion.c:654/750).
            std::int32_t* out=hokan?mka_ang:o.angles.data();
            if(!p.clip.angles[ch].empty()){const auto& a=p.clip.angles[ch][std::size_t(src)];const auto& b=p.clip.angles[ch][std::size_t(dst)];
                out[0]=std::int32_t(float(s16(a[0]))+frac*float(s16(b[0]-a[0])));
                out[1]=std::int32_t(float(flp_s)*(float(s16(a[1]))+frac*float(s16(b[1]-a[1]))));
                out[2]=std::int32_t(float(flp_s)*(float(s16(a[2]))+frac*float(s16(b[2]-a[2]))));}
            else out[0]=0; // Motion.c:663/799 assign index 0 three times
            if(hokan){flip_nearer(o.angles.data(),mka_ang);blend(o.angles.data(),rate);}
        }
        ++ch;--left;
    }
    return true;
}
}

int set_motion(MotionWork& w,int add,unsigned mode,const MotionClip* clip,const std::int8_t* datP,std::vector<ModelPose>& obj,const std::vector<std::uint32_t>& owp_flg,std::string* gap){
    if(!clip)return 0; // Motion.c:29 mnwP[mtn_no].datP == NULL
    if(!clip->frames){if(gap&&gap->empty())*gap="bhSetMotion:frm_num 0";return 0;}
    if((mode&2)&&!datP){if(gap&&gap->empty())*gap="bhSetMotion:mtn_tp fehlt bei mtn_md 0x2";return 0;}
    Pass p{*clip,(mode&2)?datP:nullptr,obj,owp_flg,gap};
    const float rate=(mode&0x20)?(1.0f/float(w.hokan_count))-(0.000015258789f*float(w.hokan_rate)):0.000015258789f*float(65536-w.hokan_rate);
    const unsigned flg=clip->flags;
    bool hokan=w.hokan_count!=0;
    if((mode&8)||(flg&0x20))slow(p,w.frm_no,mode,hokan,rate);
    else if(flg&0x10){if(hokan)normal_hokan(p,w.frm_no,mode,rate);else normal(p,w.frm_no,mode);}
    else if(hokan)fast_hokan(p,w.frm_no,mode,rate);else fast(p,w.frm_no,mode);
    if(hokan)--w.hokan_count;
    // Motion.c:71 atrP[frm_no >> 16], before the frame advances.
    if(!clip->attributes.empty()){const std::int32_t at=w.frm_no>>16;if(at>=0&&std::size_t(at)<clip->attributes.size())w.mtn_attr=clip->attributes[std::size_t(at)];else if(gap&&gap->empty())*gap="bhSetMotion:atrP ausserhalb von frm_num";}
    const std::int32_t frm_num=std::int32_t(clip->frames<<16);
    if((mode&1)&&w.frm_mode!=0)w.frm_no-=add;else w.frm_no+=add;
    if(mode&1){if(0>=w.frm_no||w.frm_no>=frm_num-65536)w.frm_mode=~w.frm_mode;}
    else {if(w.frm_no<0){w.frm_no+=frm_num;return -1;}if(w.frm_no>=frm_num){w.frm_no-=frm_num;return 1;}}
    return 0;
}
}
