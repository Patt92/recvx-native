#include "types.h"
#include <cstring>

extern "C" {
extern SYS_WORK* sys;
O_WRK* core_bh_set_object(ETTY_WORK*, int, unsigned char*);
}

int main()
{
    static O_WRK objects[4];
    std::memset(objects, 0xff, sizeof(objects));
    sys->obwp = objects;
    ETTY_WORK source{};
    source.flg = 0xa0000001; source.id = 12; source.type = 0x3456;
    source.flr_no = 2; source.mdlver = 3; source.prm1 = 4;
    source.px = 5; source.py = 6; source.pz = 7;
    source.ax = 8; source.ay = 9; source.az = 10; source.aspd = 11;
    source.hide[0] = 1; source.hide[1] = 2; source.hide[2] = 3; source.hide[3] = 4;
    unsigned char link{};
    O_WRK* object = core_bh_set_object(&source, 2, &link);
    if (object != &objects[2] || object->flg != source.flg || object->id != 12 || object->type != 0x56 || object->param != 0x34) return 1;
    if (object->mdflg != 0x50 || object->px != 5 || object->ay != 9 || object->aspd != 11 || object->sx != 1 || object->sxb != 1) return 2;
    if (object->hide[3] != 4 || object->lkwkp != &link || object->mtx != (float(*)[16])object->mtxbuf || object->clp_jno[0] != 0 || object->clp_jno[1] != -1 || object->idx_ct != 2) return 3;
}
