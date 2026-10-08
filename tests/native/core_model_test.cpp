#include "core_model.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace {
bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
}
}

int main()
{
    core_nj_init_matrix_stack();

    NJS_CNK_OBJECT objects[2]{};
    objects[0].pos[0] = 10.0f;
    objects[0].pos[1] = 20.0f;
    objects[0].pos[2] = 30.0f;
    objects[0].child = &objects[1];
    objects[1].pos[0] = 1.0f;
    objects[1].pos[1] = 2.0f;
    objects[1].pos[2] = 3.0f;

    O_WORK matrices[2]{};
    ML_WORK model{};
    model.obj_num = 2;
    model.objP = objects;
    model.owP = matrices;

    BH_PWORK enemy{};
    NJS_MATRIX root{};
    enemy.mtx = &root;
    enemy.mlwP = &model;
    enemy.px = 100.0f;
    enemy.py = 200.0f;
    enemy.pz = 300.0f;

    core_bh_calc_model(&enemy);

    assert(near(root[12], 100.0f));
    assert(near(root[13], 200.0f));
    assert(near(root[14], 300.0f));
    assert(near(matrices[0].mtx[12], 110.0f));
    assert(near(matrices[0].mtx[13], 220.0f));
    assert(near(matrices[0].mtx[14], 330.0f));
    assert(near(matrices[1].mtx[12], 111.0f));
    assert(near(matrices[1].mtx[13], 222.0f));
    assert(near(matrices[1].mtx[14], 333.0f));

    NJS_MATRIX preserved{};
    preserved[0] = preserved[5] = preserved[10] = preserved[15] = 1.0f;
    preserved[12] = 7.0f;
    preserved[13] = 8.0f;
    preserved[14] = 9.0f;
    enemy.flg = 0x1000;
    enemy.mtx = &preserved;
    model.obj_num = 0;
    core_bh_calc_model(&enemy);
    assert(near(preserved[12], 7.0f));
    assert(near(preserved[13], 8.0f));
    assert(near(preserved[14], 9.0f));
}
