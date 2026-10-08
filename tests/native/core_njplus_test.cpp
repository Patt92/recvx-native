#include "types.h"
#include "njplus.h"
#include "core_model.h"
#include <cmath>
#include <cstring>
#include <cstdio>

extern "C" {
int core_np_collision_check_cc(NJS_CAPSULE*, NJS_CAPSULE*);
int core_np_collision_check_cc_ex(NJS_CAPSULE*, NJS_CAPSULE*, NJS_POINT3*);
int core_np_collision_check_sc(NJS_SPHERE*, NJS_CAPSULE*);
void core_np_distance_p2c(NJS_POINT3*, NJS_CAPSULE*, NJS_POINT3*);
void core_np_set_memory(unsigned char*, unsigned, char);
void core_np_set_memory_l(unsigned int*, unsigned, int);
void core_nj_mem_copy4(void*, const void*, int);
void core_np_push_mdlstr(NJS_CNK_OBJECT*, int);
void core_np_pop_mdlstr(NJS_CNK_OBJECT*, int);
void core_np_push_mdlstr2(NJS_CNK_OBJECT*, int);
void core_np_pop_mdlstr2(NJS_CNK_OBJECT*, int);
unsigned core_np_get_mat_color(NJS_CNK_OBJECT*, int);
void core_np_set_all_mat_color(NJS_CNK_OBJECT*, int, unsigned);
void core_np_plus_init();
void core_np_calc_skin(void*, int, int*);
void core_np_cut_skin(BH_PWORK*, int, int, int, float, float, float);
void core_np_calc_morphing(NJS_CNK_OBJECT*, NJS_CNK_OBJECT*, float, int);
}

extern "C" unsigned char* njpmemp;

static bool near(float left, float right) { return std::fabs(left - right) < 0.0001f; }

int main()
{
    NJS_CAPSULE first{{0, 0, 0}, {0, 0, 10}, 1};
    NJS_CAPSULE second{{1.5f, 0, 0}, {1.5f, 0, 10}, 1};
    if (!core_np_collision_check_cc(&first, &second)) return 1;
    second.c1.x = second.c2.x = 2.5f;
    if (core_np_collision_check_cc(&first, &second)) return 2;

    second.c1.x = second.c2.x = 1.5f;
    NJS_POINT3 contact{};
    if (!core_np_collision_check_cc_ex(&first, &second, &contact)) return 3;
    if (!near(contact.x, 1.0f) || !near(contact.y, 0) || !near(contact.z, 0)) return 4;

    NJS_SPHERE sphere{{1.5f, 0, 5}, .75f};
    if (!core_np_collision_check_sc(&sphere, &first)) return 5;
    sphere.c.x = 2.0f;
    if (core_np_collision_check_sc(&sphere, &first)) return 6;

    NJS_POINT3 point{5, 0, 5}, surface{};
    core_np_distance_p2c(&point, &first, &surface);
    if (!near(surface.x, 1) || !near(surface.y, 0) || !near(surface.z, 5)) return 7;

    unsigned char bytes[4]{};
    core_np_set_memory(bytes, 4, (char)0xa5);
    for (unsigned char byte : bytes) if (byte != 0xa5) return 8;
    unsigned words[3]{};
    core_np_set_memory_l(words, 3, -2);
    for (unsigned word : words) if (word != 0xfffffffeu) return 9;
    int copy_source[3]{4, 5, 6};
    int copy_destination[3]{};
    core_nj_mem_copy4(copy_destination, copy_source, 3);
    if (std::memcmp(copy_destination, copy_source, sizeof(copy_source)) != 0) return 10;

    int first_vertices[]{1, 2}, second_vertices[]{3, 4};
    short first_polygons[]{5}, second_polygons[]{6};
    NJS_CNK_MODEL models[2]{};
    models[0].vlist = first_vertices; models[0].plist = first_polygons;
    models[1].vlist = second_vertices; models[1].plist = second_polygons;
    NJS_CNK_OBJECT objects[2]{};
    objects[0].model = &models[0]; objects[0].pos[0] = 1; objects[0].pos[1] = 2; objects[0].pos[2] = 3;
    objects[1].model = &models[1]; objects[1].pos[0] = 4; objects[1].pos[1] = 5; objects[1].pos[2] = 6;
    core_np_push_mdlstr(objects, 2);
    models[0].vlist = second_vertices;
    core_np_pop_mdlstr(objects, 2);
    if (models[0].vlist != first_vertices) return 11;
    core_np_push_mdlstr2(objects, 2);
    objects[0].pos[0] = 99; models[0].vlist = second_vertices; models[0].plist = second_polygons;
    objects[1].evalflags = 0x8; objects[1].pos[0] = 99;
    core_np_pop_mdlstr2(objects, 2);
    if (!near(objects[0].pos[0], 1) || models[0].vlist != first_vertices || models[0].plist != first_polygons) return 12;
    if (!near(objects[1].pos[0], 99)) return 13;

    alignas(unsigned) short material_list[]{17, 2, 0, 0x0100, 255};
    models[0].plist = material_list;
    if (core_np_get_mat_color(objects, 2) != *reinterpret_cast<unsigned*>(material_list + 2)) return 14;
    core_np_set_all_mat_color(objects, 1, 0x11223344);
    const auto* material = reinterpret_cast<const unsigned char*>(material_list + 1) + 2;
    if (material[0] != 0x44 || material[1] != 0x33 || material[2] != 0x22 || material[3] != 0x11) return 15;

    alignas(64) static unsigned char skin_memory[262144]{};
    njpmemp = skin_memory;
    core_np_plus_init();
    core_nj_init_matrix_stack();
    O_WORK bone{};
    bone.mtx[0] = bone.mtx[5] = bone.mtx[10] = bone.mtx[15] = 1;
    bone.mtx[12] = 10; bone.mtx[13] = 20; bone.mtx[14] = 30;
    alignas(64) unsigned char source_bytes[128]{};
    auto* source_header = reinterpret_cast<HDR_PS*>(source_bytes);
    source_header->ucType = 51; source_header->ucAttr = 7; source_header->usSize = 2; source_header->usIndexOfs = 3; source_header->usIndexMax = 1;
    auto* source_vertex = reinterpret_cast<NJS_POINT4*>(source_header + 1);
    *source_vertex++ = {1, 2, 3, 1};
    *source_vertex = {1, 0, 0, 0};
    NJS_CNK_MODEL skin_model{};
    skin_model.vlist = reinterpret_cast<int*>(source_header);
    NJS_CNK_OBJECT skin_object{};
    skin_object.model = &skin_model;
    ML_WORK skin_work{};
    skin_work.obj_num = 1; skin_work.objP = &skin_object; skin_work.owP = &bone;
    BH_PWORK skin_actor{};
    skin_actor.mlwP = &skin_work; skin_actor.px = 1; skin_actor.py = 2; skin_actor.pz = 3;
    int skin_program[]{2, 0, 1, 9 << 16};
    core_np_calc_skin(&skin_actor, 1, skin_program);
    auto* skinned_header = reinterpret_cast<HDR_PS*>(skin_model.vlist);
    auto* skinned_vertex = reinterpret_cast<NJS_POINT4*>(skinned_header + 1);
    if (skinned_header == source_header || skinned_header->ucType != 51 || skinned_header->usIndexMax != 1) return 16;
    if (!near(skinned_vertex[0].x, 10) || !near(skinned_vertex[0].y, 20) || !near(skinned_vertex[0].z, 30)) return 17;
    if (!near(skinned_vertex[1].x, 1) || !near(skinned_vertex[1].y, 0) || !near(skinned_vertex[1].z, 0)) return 18;
    core_np_cut_skin(&skin_actor, 1, 2, 3, 4, 5, 6);

    alignas(64) unsigned char morph_source_bytes[128]{};
    alignas(64) unsigned char morph_destination_bytes[128]{};
    auto* morph_source_header = reinterpret_cast<HDR_PS*>(morph_source_bytes);
    auto* morph_destination_header = reinterpret_cast<HDR_PS*>(morph_destination_bytes);
    morph_source_header->ucType = morph_destination_header->ucType = 51;
    morph_source_header->usIndexMax = morph_destination_header->usIndexMax = 1;
    auto* morph_source_vertex = reinterpret_cast<NJS_POINT4*>(morph_source_header + 1);
    auto* morph_destination_vertex = reinterpret_cast<NJS_POINT4*>(morph_destination_header + 1);
    morph_source_vertex[0] = {0, 0, 0, 3}; morph_source_vertex[1] = {1, 0, 0, 4};
    morph_destination_vertex[0] = {10, 20, 30, 5}; morph_destination_vertex[1] = {0, 2, 4, 6};
    NJS_CNK_MODEL morph_models[2]{};
    morph_models[0].vlist = reinterpret_cast<int*>(morph_source_header);
    morph_models[1].vlist = reinterpret_cast<int*>(morph_destination_header);
    NJS_CNK_OBJECT morph_source{};
    NJS_CNK_OBJECT morph_destination{};
    morph_source.model = &morph_models[0]; morph_destination.model = &morph_models[1];
    morph_source.pos[0] = 2; morph_destination.pos[0] = 10;
    core_np_calc_morphing(&morph_source, &morph_destination, 250, 1);
    auto* morph_output = reinterpret_cast<NJS_POINT4*>(reinterpret_cast<HDR_PS*>(morph_models[0].vlist) + 1);
    if (!near(morph_source.pos[0], 4) || !near(morph_output[0].x, 2.5f) || !near(morph_output[0].y, 5) || !near(morph_output[0].z, 7.5f)) return 19;
    if (!near(morph_output[0].w, 5) || !near(morph_output[1].x, .75f) || !near(morph_output[1].y, .5f) || !near(morph_output[1].z, 1) || !near(morph_output[1].w, 6)) return 20;
    std::puts("core njplus collision tests passed");
}
