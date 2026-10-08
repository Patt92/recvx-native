/* Host interface of the ILP32 original-engine module (tools/build_core32.py).
 *
 * The original sources run unchanged inside a 32-bit linear memory, so all work
 * structures keep their PS2 layout. The host only passes plain values in and
 * receives plain values or module addresses out; module addresses are offsets
 * into recvx_original_memory(). */
#ifndef RECVX_ORIGINAL_ENGINE_H
#define RECVX_ORIGINAL_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RECVX_ORIGINAL_HOST {
    void* user;
    /* njCnk*DrawModel: model = NJS_CNK_MODEL address, matrix = current Ninja
     * matrix (16 floats), texlist = last njSetTexture argument, mode = 0 Easy,
     * 1 EasyMulti, 2 Simple, 3 SimpleMulti (njCnkSetCurrentDrawMode). */
    void (*draw_model)(void* user, uint32_t model, const float* matrix, uint32_t texlist, int mode);
    /* sdfunc.c sound requests, original arguments. */
    void (*request_enemy_se)(void* user, uint32_t enemy, uint32_t position, int se);
    void (*system_se)(void* user, int se);
    void (*player_voice)(void* user, int voice);
    void (*vibration)(void* user, int attribute, int number);
} RECVX_ORIGINAL_HOST;

typedef struct RECVX_ORIGINAL_ENEMY {
    uint32_t flg, id, type, stflg, mdflg;
    int32_t hp, mode0, mode1, mode2, mode3;
    float px, py, pz;
    int32_t ax, ay, az;
    uint32_t mtn_no, frm_no;
} RECVX_ORIGINAL_ENEMY;

int recvx_original_init(const RECVX_ORIGINAL_HOST* host);
uint8_t* recvx_original_memory(size_t* size);
/* bhInitReadRDT, RDT into module memory, bhSetRDT, bhFinishRoom.
 * Returns rom->ene_n or -1 (sys->error & 1). */
int recvx_original_load_room(const uint8_t* rdt, size_t size);
/* sys->txlp / sys->txdp pairs queued by bhSetRoom (texture list, data address). */
int recvx_original_texture_count(void);
void recvx_original_texture(int index, uint32_t* texlist, uint32_t* data);
void recvx_original_set_player(float px, float py, float pz, int ay, int flr_no, int hp);
void recvx_original_set_system_flags(uint32_t sp_flg, uint32_t pt_flg, uint32_t st_flg, uint32_t gm_flg);
void recvx_original_control_enemy(void);
void recvx_original_draw_enemy(const float view[16]);
int recvx_original_enemy_slots(void);
void recvx_original_enemy(int index, RECVX_ORIGINAL_ENEMY* out);
/* Imports of the module that are not bound yet and were called (port gaps). */
int recvx_original_gap_count(void);
const char* recvx_original_gap(int index, unsigned* calls);

#ifdef __cplusplus
}
#endif
#endif
