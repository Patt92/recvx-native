#include "effect_sprites.h"
#include "chunk_blend.h"
#include "binary_view.h"
#include "core_random.h"
#include "effect_collision.h"
#include "ninja_math.h"
#include "ninja_matrix.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace recvx {
// Host-Typen der Effektverknüpfung; EffectSprite hält nur Zeiger darauf.
struct EffectLinkNode {NJS_MATRIX mtx{};};
struct EffectModelWork {EffectLinkNode* owP=nullptr;};
// E03-Basis: ef_info/WpnEffTab/lgttab sind reine Daten; Flamme und Licht
// sind eigenständige native Mechanismen auf dem vorhandenen Besitzer.
// PORTIERUNGSLÜCKE: Andere Effektprozeduren, Explosion/Blut/Trümmer und
// Metalldetektor bleiben ungebunden. Keine Generator-/Originalkernbrücke.
namespace {
EffectOutput pending;
// effect.c ef_info: {model block present, animation textures}. Terminal
// {-1,0} is represented by the end of the native data array.
constexpr std::pair<unsigned,unsigned> categories[]={{1,9},{1,0},{0,2},{0,4},{0,4},{0,4},{0,2},{1,0},{0,1},{0,2},{0,3},{0,5},{0,0},{1,0},{0,0},{0,0},{0,0},{0,2},{0,1},{0,1}};
// UV_WORK (types.h:2194) und EFF5UV/EFF5UVTBL (effsub5.h:6/22) als reine
// Layouttypen der Datentabellen, kein Originalcode.
struct EffectUv {float u,v,xs,ys;};
struct EffectFireUv {short u,v;};
struct EffectFireTable {const EffectFireUv* table;int count,tex_id,ani_ct,palette_bank;short w,h;};
#include "effect_sprite_tables_data.inc"
// bhGetGroundPosition-Anbieter des Raumbesitzers (kein eigener ATR-Satz).
GroundCollision* effect_ground=nullptr;
// rand() der Originalroutinen: derselbe prozessweite EE-Generator.
float effect_random(){return -recvx_core_random_next()/-2.1474836e9f;}
void quad_color(EffectSprite& s,std::uint32_t color){for(auto& v:s.tv)v.col=color;}
void quad_uv(EffectSprite& s,const EffectUv& uv){
    s.tv[0].u=uv.u;       s.tv[0].v=uv.v;
    s.tv[1].u=uv.u+uv.xs; s.tv[1].v=uv.v;
    s.tv[2].u=uv.u;       s.tv[2].v=uv.v+uv.ys;
    s.tv[3].u=uv.u+uv.xs; s.tv[3].v=uv.v+uv.ys;
}
const EffectUv& latched_uv(const EffectSprite& s,int index,const char* what){
    if(!s.uv)throw std::runtime_error(std::string(what)+" without its original UV table");
    if(index<0)throw std::runtime_error(std::string(what)+" UV cursor below its original table");
    return static_cast<const EffectUv*>(s.uv)[index];
}
// effsub0.c:10 setentry: eine Effektanforderung mit Lage/Winkel/Skalierung des Erzeugers.
void setentry(unsigned id,unsigned type,const EffectSprite& s){
    EffectSpawn e;e.flg=0x4100001;e.id=id;e.type=type;e.flr_no=0;e.mdlver=0;
    e.position={s.px,s.py,s.pz};e.scale={s.sx,s.sy,s.sz};e.ax=s.ax;e.ay=s.ay;
    pending.spawns.push_back(e);
}
// effsub0.c:29 effinit. op->bank (sys->ef_pbkb[tex_id]) ist die PVP-Palettenbank
// der Originalhardware; der native Zeichenpfad liest dekodierte RGBA-Texturen,
// deshalb hat die Bank hier keinen Abnehmer.
void effinit(EffectSprite& s){
    s.flg|=0x4100000;
    s.tv[0]={-1,-1,0};s.tv[1]={1,-1,0};s.tv[2]={-1,1,0};s.tv[3]={1,1,0};
    quad_color(s,0xffffffffu);
    s.bl_src=8;s.bl_dst=6;
    s.ani_ct=0;s.ct1=0;s.ct0=0;
    s.sxb=s.sx;s.syb=s.sy;
    s.spd=0;s.zn=0;s.yn=0;s.xn=0;
}
// effsub0.c:63 effset: Groesse aus der UV-Kachel und die vier Eckfarben.
void effset(EffectSprite& s,const EffectUv& uv,unsigned num){
    if(num>=10)throw std::runtime_error("bhEff182 smoke colour outside the original kcol table");
    s.sx=4.0f*(s.sxb*uv.xs);
    s.sy=4.0f*(s.syb*uv.ys);
    for(unsigned i=0;i<4;++i)s.tv[i].col=effset_color[num][i];
    quad_uv(s,uv);
}
// effsub1.c:8158 bhEff122 — weltfeste Leuchtplatte, Texturgruppe 41.
// ani_ct waehlt die Platte (type), mode1 die UV-Kachel (Skriptbefehl 0x91).
void step_eff122(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=41;s.flg|=0x4000000;s.bl_src=8;s.bl_dst=6;
        quad_color(s,0xffffffffu);
        // exp0 zeigt auf uvinfo_p0; die Raumskripte adressieren damit auch die
        // direkt folgenden Originalzeilen (siehe Datenmodul, 0x00326B60).
        s.ct0=0;s.uv=eff122_uv_window;s.mode0=1;
    }
    s.ani_ct=s.type;
    if(s.mode1>=22)throw std::runtime_error("bhEff122 mode1 outside the original uvinfo rodata window");
    quad_uv(s,latched_uv(s,int(s.mode1),"bhEff122"));
    queued=true;
}
// effsub1.c:8208 bhEff123 — blinkende Farblampe, gemeinsame Textur 2.
void step_eff123(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=2;s.flg|=0x4100000;s.bl_src=8;s.bl_dst=10;
        s.ani_ct=0;s.ct0=0;s.uv=eff123_uv;s.ct2=0;s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct1;
        if(s.ct1>s.ct2||s.mode1!=0){
            s.ct1=0;s.ct2=int(20.0f+10.0f*effect_random());
            s.mode1=0;s.mode0=2;
        }
        return;
    }
    switch(s.type){
    case 1:quad_color(s,0xffff0000u);break;
    case 2:quad_color(s,0xff00ff00u);break;
    case 3:quad_color(s,0xff0000ffu);break;
    case 4:quad_color(s,0xff00ffffu);break;
    case 5:quad_color(s,~0xffu);break;
    default:quad_color(s,0xffffffffu);break;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff123");
    s.az=s.ct0*4096;
    quad_uv(s,uv);
    s.ct0=(s.ct0+1)&3;
    if(s.ct0==0)s.mode0=1;
    queued=true;
}
// effsub1.c:8458 bhEff125 — Erzeuger; das Skript gibt ihn ueber mode1 frei.
void step_eff125(EffectSprite& s,const EffectContext& c){
    s.flg|=0x1000000;
    if((c.gm_flg&1u)||s.mode1==0)return;
    EffectSpawn e;e.id=126;e.flg=0x4100001;e.mdlver=s.type%4;e.type=s.mode1-1;
    e.scale={0.25f*s.sx,0.25f*s.sy,s.sz};
    const float jitter_x=effect_random()-0.5f,jitter_z=effect_random()-0.5f;
    e.position={s.px+jitter_x,s.py,s.pz+jitter_z};
    e.ay=s.ay;
    pending.spawns.push_back(e);
    s.mode1=0;
}
// effsub1.c:8488 bhEff126 — aufsteigende Dampf-/Staubwolke, Texturgruppe 35.
void step_eff126(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=35;s.flg|=0x4100000;
        if(s.type<5){
            s.tv[0].x=-1.0f;s.tv[0].y= 1.0f;s.tv[0].z=0;
            s.tv[1].x= 1.0f;s.tv[1].y= 1.0f;s.tv[1].z=0;
            s.tv[2].x=-1.0f;s.tv[2].y=-1.0f;s.tv[2].z=0;
            s.tv[3].x= 1.0f;s.tv[3].y=-1.0f;s.tv[3].z=0;
            quad_color(s,0xffffffffu);
        }
        s.bl_src=8;s.bl_dst=6;
        if(!effect_ground)throw std::runtime_error("bhEff126 without the room's bhGetGroundPosition provider");
        s.gpy=effect_ground->query({s.px,s.py-3.0f,s.pz});
        s.yn=-0.1f;
        switch(s.type){
        case 0:case 1:
            s.uv=eff126_uv[0];s.spd=0.015f*s.sz;s.ani_ct=0;break;
        case 2:case 3:
            s.uv=eff126_uv[1];s.sx*=0.8f;s.sy*=0.8f;s.spd=0.02f*s.sz;s.ani_ct=1;break;
        case 4:
            quad_color(s,((64u-(s.mdlver*16u))<<24)|0xffffffu);
            s.uv=eff126_uv[2];s.sx*=1.2f;s.sy*=1.2f;s.spd=0.005f*s.sz;s.ani_ct=2;break;
        case 6:
            s.yn=0.25f*s.sz;
            [[fallthrough]];
        case 5:
            quad_color(s,((128u-(s.mdlver*32u))<<24)|0xffffffu);
            s.uv=eff126_uv[3];s.ani_ct=2;break;
        default:
            throw std::runtime_error("bhEff126 type outside the original uvinfo selection");
        }
        s.mode0=1;
    }
    switch(s.type){
    case 0:case 1:case 2:case 3:case 4:
        if(s.yn>-3.0f)s.yn-=s.spd;
        s.py+=s.yn;break;
    case 6:
        s.py+=s.yn;s.yn*=0.8f;break;
    default:break;
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff126");
    if(uv.u==-1.0f||s.py<s.gpy){s.flg=0;return;}
    quad_uv(s,uv);
    ++s.ct0;
    queued=true;
}
// effsub0.c:1916 bhEff181 — Raucherzeuger; floor(sz) ist der Abstand der Stoesse.
void step_eff181(EffectSprite& s){
    s.flg|=0x1000000;
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0)return;
    const unsigned typ=s.type-1;
    if(std::floor(s.sz)==0.0f){
        setentry(182,typ,s);
        s.mode1=0;s.type=0;return;
    }
    if(s.ct1<1){
        setentry(182,typ,s);
        s.ct1=int(std::floor(s.sz));
    }
    --s.ct1;
}
// effsub0.c:1952 bhEff182 — Rauchwolke, Texturgruppen 400..407.
// type: Farbsatz type%10, Form type/10. njFraction(sz) ist der Vorschub.
void step_eff182(EffectSprite& s,bool& queued){
    const unsigned kcolor=s.type%10,ktype=s.type/10;
    if(s.mode0==0){
        s.tex_id=eff182_texid[kcolor];
        effinit(s);
        s.uv=eff182_kemu[kcolor];
        s.mode0=1;
    }
    NJS_MATRIX basis;njUnitMatrix(&basis);njRotateXYZ(&basis,s.ax,s.ay,0);
    NJS_POINT3 step{0,0,njFraction(s.sz)},moved;njCalcVector(&basis,&step,&moved);
    s.px+=moved.x;s.py+=moved.y;s.pz+=moved.z;
    const auto& uv=latched_uv(s,s.ct0,"bhEff182");
    if(uv.u==-1.0f){s.flg=0;return;}
    effset(s,uv,ktype);
    ++s.ct0;
    queued=true;
}
// effsub1.c:731 bhEff002 — Rauch/Staub-Sprite (Atlas-Gruppe 2). mdlver waehlt die UV-Tabelle,
// type das Verhalten. Die Nachfolgeanforderungen laufen ueber bhSetEffectTb (setentry-Muster).
float eff002_random(){return float(-recvx_core_random_next())/-2147483648.0f;}
void eff002_request(EffectSprite& s,unsigned type,unsigned mdlver){
    EffectSpawn e;e.flg=0x4100001;e.id=2;e.type=type;e.flr_no=0;e.mdlver=mdlver;
    e.position={s.px,s.py,s.pz};e.scale={s.sxb,s.syb,s.szb};e.ax=0;e.ay=0;e.lkono=s.ct0;
    pending.spawns.push_back(e);
}
void step_eff002(EffectSprite& s,bool& queued){
    if(s.mode0==0){
        s.tex_id=2;quad_color(s,0x80ffffffu);
        switch(s.type){
        case 2:quad_color(s,0x60c0c0c0u);[[fallthrough]];
        case 0:case 6:s.bl_src=8;s.bl_dst=6;s.yn=0;break;
        case 3:
            s.xn=float(double(eff002_random())*0.1-0.05f);
            s.yn=float(0.1f+double(eff002_random())*0.1);
            s.zn=float(double(eff002_random())*0.1-0.05f);
            [[fallthrough]];
        case 5:
            quad_color(s,s.mdlver==11?0x40ffffffu:0x40c0c0c0u);
            s.ani_ct=1;s.bl_src=8;s.bl_dst=6;break;
        case 4:s.bl_src=8;s.bl_dst=6;s.yn=0;s.ani_ct=1;break;
        case 7:quad_color(s,0x40ffffffu);s.bl_src=8;s.bl_dst=10;s.spd=64.0f;s.ani_ct=1;break;
        case 8:
            s.xn=s.lox;s.yn=s.loy;s.zn=s.loz;
            quad_color(s,0x40ffffffu);s.ani_ct=1;s.bl_src=8;s.bl_dst=10;break;
        case 1:
            s.flg=0x4100001;s.bl_src=11;s.bl_dst=3;
            s.xn=float(double(eff002_random())*0.2-0.1f);
            s.yn=float(0.4f+double(eff002_random())*0.3);
            s.zn=float(double(eff002_random())*0.2-0.1f);
            s.ani_ct=1;break;
        case 9:
            s.xn=float(double(eff002_random())*0.1-0.05f);
            s.yn=s.sz;
            s.zn=float(double(eff002_random())*0.1-0.05f);
            quad_color(s,0xc0202020u);s.ani_ct=1;s.bl_src=8;s.bl_dst=6;break;
        case 10:s.bl_src=8;s.bl_dst=6;s.ct0=s.lkono;s.ct3=128;break;
        default:break;
        }
        switch(s.mdlver){
        case 1:s.uv=eff002_v1;break;
        case 2:case 11:s.uv=eff002_v2;break;
        case 3:s.uv=eff002_v3;break;
        case 4:s.uv=eff002_v4;break;
        case 8:s.uv=eff002_v8;break;
        case 9:s.uv=eff002_v9;break;
        case 10:s.uv=eff002_vz;break;
        default:s.uv=eff002_v0;break;
        }
        s.mode0=1;
        if(s.type<9)return;
    }else if(s.mode0==1){
        switch(s.type){
        case 0:
            if(s.ct0<4)eff002_request(s,10,10);
            [[fallthrough]];
        case 6:
            s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
            s.lox*=0.9f;s.loy*=0.9f;s.loz*=0.9f;
            s.py+=s.yn;
            if(s.yn<0.1f)s.yn+=0.01f;
            break;
        case 3:case 8:
            if(s.flg&0x80){
                eff002_request(s,3,11);
                --s.flr_no;
                if(s.flr_no<=0)s.flg=0;
                return;
            }
            s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
            s.yn=s.yn*0.9f;
            break;
        case 4:
            if(s.ct0<6)eff002_request(s,10,3);
            s.px+=s.lox;s.py+=s.loy;s.pz+=s.loz;
            s.lox*=0.9f;s.loy*=0.9f;s.loz*=0.9f;
            break;
        case 1:case 9:
            s.px+=s.xn;s.py+=s.yn;s.pz+=s.zn;
            s.yn=s.yn*0.9f;
            break;
        case 7:
            quad_color(s,(unsigned(s.spd)<<24)|0xffffffu);
            s.spd*=0.9f;
            break;
        case 10:
            quad_color(s,(unsigned(s.ct3)<<24)|0xffffffu);
            s.ct3-=32;
            if(s.ct3<=0){s.flg=0;return;}
            break;
        default:break;
        }
    }
    const auto& uv=latched_uv(s,s.ct0,"bhEff002");
    if(uv.u==-1.0f){s.flg=0;return;}
    s.sx=4.0f*(s.sxb*uv.xs);
    s.sy=4.0f*(s.syb*uv.ys);
    quad_uv(s,uv);
    if(s.type<10)++s.ct0;
    queued=true;
}
// effsub5.c:1609 bhEff218 — Feuer. 17 Varianten auf den Texturgruppen 70..79.
void step_eff218(EffectSprite& s,bool& queued){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000;return;}
    s.flg&=~0x1000000u;
    const auto& fire=eff218_tables[(s.type-1)%17];
    if(s.mode0==0){
        s.flg|=0x4180000;s.bl_src=8;s.bl_dst=3;
        s.tv[0].x=s.tv[2].x=-1.0f;s.tv[1].x=s.tv[3].x=1.0f;
        s.tv[0].y=s.tv[1].y=-2.0f;s.tv[2].y=s.tv[3].y=0;
        for(auto& v:s.tv)v.z=0;
        quad_color(s,0xffffffffu);
        s.ct0=int(float(fire.count)*effect_random());
        s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct0;
        if(fire.count<=s.ct0)s.ct0=0;
    }
    s.tex_id=fire.tex_id;s.ani_ct=unsigned(fire.ani_ct);
    if(s.ct0<0||s.ct0>=fire.count)throw std::runtime_error("bhEff218 frame outside its original fire table");
    const auto& info=fire.table[s.ct0];
    s.tv[0].u=s.tv[2].u=float(info.u)/256.0f;
    s.tv[1].u=s.tv[3].u=float((info.u+fire.w)-1)/256.0f;
    s.tv[0].v=s.tv[1].v=info.v?float(info.v+1)/256.0f:float(info.v)/256.0f;
    s.tv[2].v=s.tv[3].v=float(info.v+fire.h)/256.0f;
    queued=true;
}
// effsub5.c:3333 bhEff230 — animierte Leuchttafel, Texturgruppe 414.
// lkono waehlt die UV-Bank, (type-1)%6 die Tafel.
void step_eff230(EffectSprite& s,bool& queued){
    if(s.type==0&&s.mode1!=0)s.type=s.mode1;
    if(s.type==0){s.flg|=0x1000000;return;}
    s.flg&=~0x1000000u;
    if(s.lkono<0)throw std::runtime_error("bhEff230 link node below the original UV bank table");
    const unsigned bank=unsigned(s.lkono)%4;
    if(s.mode0==0){
        s.flg|=0x4080000;s.tex_id=414;s.bl_src=8;s.bl_dst=6;s.ct0=0;
        quad_color(s,0xffffffffu);
        s.mode0=1;
    }else if(s.mode0==1){
        ++s.ct0;
        if(int(eff230_uv_max[bank])<=s.ct0)s.ct0=0;
    }
    s.ani_ct=(s.type-1)%6;
    if(s.ct0<0||s.ct0>=int(eff230_uv_max[bank]))throw std::runtime_error("bhEff230 frame outside its original UV bank");
    const auto& info=eff230_uv[bank][s.ct0];
    s.tv[0].u=s.tv[2].u=float(info.u)/256.0f;
    s.tv[1].u=s.tv[3].u=float(info.u+31)/256.0f;
    s.tv[0].v=s.tv[1].v=float(info.v)/256.0f;
    s.tv[2].v=s.tv[3].v=float(info.v+31)/256.0f;
    queued=true;
}
}
void set_effect_context(const EffectContext& context){(void)context;}
LighterSprite lighter_sprite(){
    EffectSpawn spawn;spawn.id=9;spawn.type=2;spawn.scale={.5f,.5f,.5f};
    LighterSprite out;initialize_effect_sprite(out.sprite,spawn);out.scale=spawn.scale;return out;
}
OriginalLight lighter_light(){
    // lgttab[1]. bhSetLightTab replaces its tabular direction with
    // bhGetLightVector(0,0,0), i.e. (0,0,-1).
    return {1,4,30,1,0,9,4,{0,0,0},{.3f,0,-1.5f},{0,0,-1},0,0,0,{3.16f,2.06f,.7f},8,400};
}
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& data){
    if(data.empty())return std::size_t(-1);
    const BinaryView bytes(data);std::size_t cursor=0;
    // Follow exactly the model-bearing categories, not a magic offset or a
    // byte search for PVP. Each model size is followed by the next size word.
    for(const auto [model,frames]:categories)if(model){
        const auto size=bytes.u32(cursor);cursor+=4;
        if(size&0x80000000u)throw std::runtime_error("common effect model chain ends before ef_info");
        bytes.require(cursor,size);cursor+=size;
    }
    bytes.require(cursor,4);cursor+=4; // word fetched after the final model
    const auto offset=(cursor+31)&~std::size_t(31);bytes.require(offset,4);return offset;
}
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(){
    std::vector<std::pair<unsigned,unsigned>> out;unsigned first=0;
    for(const auto [model,frames]:categories){out.emplace_back(first,frames);first+=frames;}return out;
}
bool supported_effect_sprite(unsigned id){
    switch(id){
    case 2:case 9:case 122:case 123:case 125:case 126:case 181:case 182:case 218:case 230:return true;
    default:return false;
    }
}
bool ported_effect_atlas(unsigned group){
    return group==35||group==41||group==414||(group>=70&&group<=79)||(group>=400&&group<=407);
}
bool step_effect_sprite(EffectSprite& sprite,bool* queued){
    if(supported_effect_sprite(sprite.id))throw std::runtime_error("flame step requires native flag context");
    if(queued)*queued=false;return false;
}
bool step_effect_sprite(EffectSprite& s,const EffectContext& c,bool* queued){
    if(queued)*queued=false;if(!supported_effect_sprite(s.id))return false;
    if(s.id!=9){
        bool entered=false;
        switch(s.id){
        case 2:step_eff002(s,entered);break;
        case 122:step_eff122(s,entered);break;
        case 123:step_eff123(s,entered);break;
        case 125:step_eff125(s,c);break;
        case 126:step_eff126(s,entered);break;
        case 181:step_eff181(s);break;
        case 182:step_eff182(s,entered);break;
        case 218:step_eff218(s,entered);break;
        case 230:step_eff230(s,entered);break;
        default:return false;
        }
        if(queued)*queued=entered;
        return true;
    }
    if(!s.mode0){
        s.flg|=0x4100000;s.bl_src=8;s.bl_dst=6;s.ani_ct=0;s.ct0=0;
        s.flame_compact_uv=s.type>=1&&s.type<=3;s.tex_id=s.flame_compact_uv?8:9;
        for(unsigned i=0;i<4;++i){auto& v=s.tv[i];v.x=(i&1)?1.f:-1.f;v.y=(i&2)?0.f:-2.f;v.z=0;v.col=0xffe0e0e0;}
        s.mode0=1;
    }
    if(s.type==3&&!(c.st_flg&0x20000000u)){s.flg=0;return true;}
    if(s.type==2){
        if(!(c.player_flg&0x1000000u)){s.flg=0;return true;}
        if(!(c.pt_flg&1u)||(c.player_stflg&0x1000000u)||(c.player_mdflg&1u))return true;
        s.flg&=~0x1000000u;
    }
    if(s.ct0==8)s.ct0=0;
    if(s.ct0<0||s.ct0>7)throw std::runtime_error("flame UV cursor outside native bank");
    const float edge=s.flame_compact_uv?.0625f:.25f;
    const float u=float(s.flame_compact_uv?s.ct0:s.ct0%4)*edge;
    const float v=s.flame_compact_uv?.8125f:float(s.ct0/4)*edge;
    for(unsigned i=0;i<4;++i){s.tv[i].u=u+((i&1)?edge:0);s.tv[i].v=v+((i&2)?edge:0);}
    ++s.ct0;if(queued)*queued=true;return true;
}
void step_light_color(unsigned type,unsigned speed,unsigned& clock,NJS_POINT3 color,NJS_POINT3& working,bool running){
    if(!running)return;
    working=color;if(type<1||type>6)return;
    const unsigned shape=(type-1)%3;const float wave=njSin(clock)*(shape==0?.25f:shape==1?.5f:1.f);
    if(shape==2)working={wave*color.x,wave*color.y,wave*color.z};
    else {const float base=shape==0?.75f:.5f;working={wave*color.x+color.x*base,wave*color.y+color.y*base,wave*color.z+color.z*base};}
    unsigned delta=speed<<8;
    if(type>=4)delta=unsigned(float(delta)*(-recvx_core_random_next()/-2.1474836e9f));
    clock=(clock+delta)&0x7fff;
}
void set_effect_walls(const std::vector<CollisionWall>* walls,float height){(void)walls;(void)height;}
void set_effect_ground(GroundCollision* ground,const std::vector<CollisionWall>* room,const std::vector<CollisionWall>* mobile){
    // bhGetGroundPosition der Raumwerke; Wassersaetze haben hier keinen Abnehmer.
    effect_ground=ground;(void)room;(void)mobile;
}
void set_effect_link_matrix(unsigned node,const float* matrix){
    // Die Knotenmatrix hat ohne Effektprozeduren keinen Abnehmer.
    (void)node;(void)matrix;
}
void set_explosion(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
void set_explosion_effect(NJS_POINT3 position,NJS_POINT3 view){(void)position;(void)view;}
EffectOutput take_effect_output(){EffectOutput out=std::move(pending);pending=EffectOutput{};return out;}
// bhSetEffectTb: copy flg/id/type/mdlver/flr_no, position, scale (also kept in sxb..), angles and the joint offset.
void initialize_effect_sprite(EffectSprite& s,const EffectSpawn& e){
    s=EffectSprite{};s.flg=e.flg;s.id=e.id;s.type=e.type;s.tex_id=-1;s.mdlver=e.mdlver;s.flr_no=e.flr_no;s.px=e.position.x;s.py=e.position.y;s.pz=e.position.z;
    s.sx=s.sxb=e.scale.x;s.sy=s.syb=e.scale.y;s.sz=s.szb=e.scale.z;s.ax=e.ax;s.ay=e.ay;s.az=0;s.lkono=e.lkono;
    s.lkwkp=e.linked_work;
    if(e.has_offset){s.lox=e.offset.x;s.loy=e.offset.y;s.loz=e.offset.z;}
}
// effect.c:bhSetEffectTb scans the shared eff[512] pool and accepts the first
// slot whose flg has neither allocation bit set. Missing tail entries are zeroed.
int original_effect_slot(const std::vector<unsigned>& flags){for(unsigned i=0;i<512;++i)if(i>=flags.size()||!(flags[i]&3u))return int(i);return -1;}
void append_custom_effect(Scene& scene,const EffectSprite& sprite,const SceneCamera& camera,const std::map<unsigned,unsigned>& bases){for(const auto& q:sprite.quads){const auto base=bases.find(q.category);if(base==bases.end())throw std::runtime_error("original custom-effect texture missing");EffectSprite quad;quad.id=302;quad.flg=0x100000;quad.tv=q.vertices;quad.bl_src=int(q.source);quad.bl_dst=int(q.destination);append_effect_sprite(scene,quad,q.center,{1,1,1},camera,base->second);}}
void append_effect_sprite(Scene& scene,const EffectSprite& s,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture,const float* linked_matrix){
    if((s.flg|s.stflg)&0x1000000)return;
    // bhDrawTrsEffect3D: translation, UnitRotPortion for flag 0x100000,
    // Z rotation, scale. Invert the room view rotation to place the billboard.
    NJS_MATRIX basis;njUnitMatrix(&basis);
    if(s.flg&0x100000){njRotateY(&basis,-camera.angles[1]);njRotateX(&basis,-camera.angles[0]);njRotateZ(&basis,-camera.angles[2]);njScale(&basis,1,-1,-1);njRotateZ(&basis,s.az);}
    else {if((s.flg&0x80)&&!(s.flg&0x200000)){if(!linked_matrix)throw std::runtime_error("linked effect draw missing native node matrix");std::copy(linked_matrix,linked_matrix+16,basis);}njRotateXYZ(&basis,s.ax,s.ay,s.az);}
    std::array<SceneVertex,4> vertices;
    for(unsigned i=0;i<4;++i){NJS_POINT3 point{s.tv[i].x*scale.x,s.tv[i].y*scale.y,s.tv[i].z*scale.z};njCalcVector(&basis,&point,&point);point.x+=position.x;point.y+=position.y;point.z+=position.z;vertices[i]={point,s.tv[i].u,s.tv[i].v,{}};}
    for(auto indices:{std::array<unsigned,3>{0,1,2},std::array<unsigned,3>{1,3,2}}){SceneTriangle triangle;for(unsigned i=0;i<3;++i)triangle.vertices[i]=vertices[indices[i]];triangle.color=s.tv[0].col;triangle.texture=texture;triangle.textured=true;triangle.camera_mask=false;triangle.world_lit=true;triangle.depth_write=false;
        // Effects bound with their original njColorBlendingMode pair (hit blood, explosion). The older atlas sprites keep the
        // additive approximation they were verified with until their pairs are checked against the original look.
        if(s.id==5||s.id==23||s.id==302||supported_effect_sprite(s.id)){triangle.blend=SceneBlend::Ninja;triangle.blend_register=ninja_blend_register(unsigned(s.bl_src),unsigned(s.bl_dst));}else triangle.blend=SceneBlend::Additive;
        if(s.id==9)triangle.texture_filter=(s.flg&0x20000000u)?SceneTextureFilter::Point:SceneTextureFilter::Linear;
        scene.triangles.push_back(triangle);}
}
}
