#include "types.h"
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
}

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
    std::puts("core njplus collision tests passed");
}
