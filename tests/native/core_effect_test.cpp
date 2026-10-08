#include "types.h"
#include "recvx_ptr32.h"

#include <cstring>

extern "C" {
extern O_WRK eff[512];
extern SYS_WORK* sys;
int core_bh_set_effect(int, POINT*, unsigned char*, int);
int core_bh_set_effect_tb(EF_WORK*, NJS_POINT3*, unsigned char*, int);
O_WRK* core_bh_set_extra_effect_work();
unsigned core_bh_get_gidx(O_WRK*);
int core_bh_set_shadow(char*, unsigned char*, int, float, float, float);
void core_bh_link_blood(BH_PWORK*, int, NJS_POINT3*, float);
}

int main()
{
    std::memset(eff, 0, sizeof(eff));
    std::memset(sys, 0, sizeof(*sys));
    recvx_p32_reset();

    POINT point{};
    point.px = 1; point.py = 2; point.pz = 3;
    point.ox = 4; point.oy = 5; point.oz = 6;
    unsigned char link{};
    if (core_bh_set_effect(9, &point, &link, 7) != 0) return 1;
    auto& direct = eff[0];
    if (direct.flg != 0x81 || direct.id != 9 || direct.type != 0 || direct.tex_id != -1 || direct.lkwkp != &link || direct.lkono != 7) return 2;
    if (direct.px != 1 || direct.lox != 4 || direct.sx != 1 || direct.pn != 4 || direct.tv[3].u != 1 || direct.tv[3].v != 1) return 3;

    EF_WORK template_work{};
    template_work.flg = 2; template_work.id = 3; template_work.type = 8;
    template_work.mdlver = 4; template_work.flr_no = 5;
    template_work.px = 10; template_work.py = 11; template_work.pz = 12;
    template_work.sx = 2; template_work.sy = 3; template_work.sz = 4;
    template_work.ax = 22; template_work.ay = 33;
    NJS_POINT3 offset{7, 8, 9};
    if (core_bh_set_effect_tb(&template_work, &offset, nullptr, 6) != 1) return 4;
    auto& templated = eff[1];
    if (templated.flg != 2 || templated.id != 3 || templated.type != 8 || templated.px != 10 || templated.sxb != 2 || templated.az != 0 || templated.lox != 7 || templated.lkwkp != nullptr) return 5;
    if (core_bh_set_extra_effect_work() != &eff[2] || eff[2].flg != 2) return 6;

    NJS_TEXMEMLIST texture_memory{};
    texture_memory.globalIndex = 0x1234;
    NJS_TEXNAME texture{};
    texture.texaddr = recvx_p32_intern(&texture_memory);
    sys->ef_tlist.textures = &texture;
    sys->ef_tn[0] = 0;
    O_WRK textured{};
    textured.tex_id = 0;
    if (core_bh_get_gidx(&textured) != 0x1234) return 7;

    std::memset(eff, 0, sizeof(eff));
    char joints[]{3, 4, -1};
    if (core_bh_set_shadow(joints, &link, 8, 2, 3, 4) != 0) return 8;
    if (eff[0].flg != 0x240081 || eff[0].id != 1 || eff[0].type != 0 || eff[0].jno[0] != 3 || eff[0].jno[1] != 4 || eff[0].jno[2] != -1 || eff[0].lkono != 8) return 9;
    if (core_bh_set_shadow(nullptr, nullptr, 0, 1, 1, 1) != 1 || eff[1].type != 1 || eff[1].jno[0] != -1) return 10;
    BH_PWORK blood{};
    core_bh_link_blood(&blood, 1, &offset, .5f);

    for (auto& work : eff) work.flg = 1;
    if (core_bh_set_effect(1, &point, nullptr, 0) != -1 || core_bh_set_extra_effect_work() != nullptr) return 11;
}
