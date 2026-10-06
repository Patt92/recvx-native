#pragma once
#include "scene.h"
namespace recvx {
// Fields consumed by the original bhEff200/201/218 procedures and draw path.
struct EffectSprite {
    unsigned id=0,type=0,mode0=0,mode1=0,flg=0,ani_ct=0;
    int ax=0,ay=0,az=0,lkono=0,ct0=0,tex_id=-1,bl_src=8,bl_dst=6;
    struct Vertex {float x=0,y=0,z=0,u=0,v=0;std::uint32_t col=0;};
    std::array<Vertex,4> tv{{{-1,-1,0},{1,-1,0},{-1,1,0},{1,1,0}}};
    unsigned char* exp0=nullptr; // original UV table pointer (bhEff009)
};
// Globals read by the original procedures (sys->st_flg/pt_flg, plp->flg/stflg/mdflg).
struct EffectContext {unsigned st_flg=0,pt_flg=1,player_flg=0,player_stflg=0,player_mdflg=0;};
void set_effect_context(const EffectContext& context);
// bhControlPlayer lighter (wpnr_no 1): WpnEffTab[1][0] and lgttab[1].
struct LighterSprite {EffectSprite sprite;NJS_POINT3 scale;};
LighterSprite lighter_sprite();
struct OriginalLight {unsigned flg,type;int aspd,lkflg,lkno,lkono,lsrc;NJS_POINT3 position,offset,direction;float spc,dif,amb;NJS_POINT3 color;float near_range,far_range;};
OriginalLight lighter_light();
// bhInitEffect: SYSTEM entry 1 holds the ef_info models, then the common effect PVP list.
std::size_t common_effect_textures_offset(const std::vector<std::uint8_t>& system_entry);
// sys->ef_tn: first texture of each ef_info category (bhDrawTrsEffect3D uses ef_tn[tex_id]+ani_ct).
std::vector<std::pair<unsigned,unsigned>> common_effect_categories(); // {first texture, frames}
// Runs the original procedure; `queued` reports whether it entered sys->ef_trs this frame
// (bhDrawTrsEffect3D only draws queued work).
bool step_effect_sprite(EffectSprite& sprite,bool* queued=nullptr);
bool supported_effect_sprite(unsigned id);
void append_effect_sprite(Scene& scene,const EffectSprite& sprite,NJS_POINT3 position,NJS_POINT3 scale,const SceneCamera& camera,unsigned texture);
}
