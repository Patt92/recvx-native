/* Entry points of the ILP32 original-engine module (tools/build_core32.py).
 *
 * This file is compiled into the 32-bit module next to the original sources, so
 * it sees the original work structures with their PS2 layout. The host calls
 * these exports with plain values; it never interprets module structures
 * itself. Every step names the original routine whose order it follows.
 */
#include "main.h"
#include "ps2_NaMatrix.h"
#include "njplus.h"

extern void bhInitReadRDT(void);
extern void bhSetRDT(void);
extern void bhFinishRoom(void);
extern void bhControlEnemy(void);
extern void bhDrawEnemy(void);
extern unsigned char* bhGetFreeMemory(int size, int param);

/* main.c:136 syMalloc(12845056); the PS2 heap is part of the module memory. */
static unsigned char core32_freemem[12845056] __attribute__((aligned(64)));
/* main.c:129 njInitMatrix(&mbuf, 128, 0). */
static NJS_MATRIX core32_mbuf[128];

/* main.c:136-144 and system.c bhInitSystem (memory layout, obwp/itwp). The
 * hardware/video/sound initialisation of those routines belongs to the host. */
void core32_init(void)
{
    hws->vtx_total = 0; /* system.c:50 bhCalcVtxBuffer returns 0 */
    npSetMemory((unsigned char*)sys, sizeof(SYS_WORK), 0);
    freemem = core32_freemem;
    njpmemp = freemem + (12255232 - hws->vtx_total);
    vwbmemp = freemem + (12517376 - hws->vtx_total);
    vebmemp = freemem + (12845056 - hws->vtx_total);
    njInitMatrix(core32_mbuf, 128, 0);
    npPlusInit();
    sys->memp = freemem;
    sys->endp = njpmemp;
    sys->memp += (int)&sys->save_end - (int)&sys->version;
    keepmem = sys->mempb = sys->memp;
    sys->loop_ct = 1;
    sys->memp = keepmem;
    sys->obwp = (O_WRK*)bhGetFreeMemory(39936, 32);
    sys->itwp = (O_WRK*)bhGetFreeMemory(39936, 32);
    sys->mempb = sys->memp;
}

/* system.c:1629-1702: bhInitReadRDT, then the expanded RDT lies at sys->memp.
 * Returns the module address the host copies the expanded RDT to. */
unsigned int core32_room_begin(unsigned int size)
{
    bhInitReadRDT();
    sys->memp = (unsigned char*)(((unsigned int)sys->memp + 63) & ~63u);
    sys->rdtp = sys->memp;
    sys->rdtsz = size;
    return (unsigned int)sys->rdtp;
}

/* system.c:1700-1723: memp += rdtsz; bhSetRDT; textures; bhFinishRoom. */
int core32_room_finish(void)
{
    sys->memp += sys->rdtsz;
    bhSetRDT();
    if (sys->error & 0x1)
        return -1;
    bhFinishRoom();
    return rom->ene_n;
}

/* Number of texture lists bhSetRoom queued (sys->txlp/txdp, system.c:1708). */
int core32_texture_count(void) { return sys->txr_n; }
unsigned int core32_texture_list(int i) { return (unsigned int)sys->txlp[i]; }
unsigned int core32_texture_data(int i) { return (unsigned int)sys->txdp[i]; }

/* Host-side player state the enemy routines read through plp. */
void core32_set_player(float px, float py, float pz, int ay, int flr_no, int hp)
{
    plp->px = px; plp->py = py; plp->pz = pz;
    plp->ay = ay;
    plp->flr_no = flr_no;
    plp->hp = hp;
}

void core32_set_system_flags(unsigned int sp_flg, unsigned int pt_flg, unsigned int st_flg, unsigned int gm_flg)
{
    sys->sp_flg = sp_flg;
    sys->pt_flg = pt_flg;
    sys->st_flg = st_flg;
    sys->gm_flg = gm_flg;
}

/* game.c:48 bhControlEnemy inside bhMainSequence. */
void core32_control_enemy(void) { bhControlEnemy(); }

/* Host-written view matrix (cam.mtx contents) and a zeroed PDS_PERIPHERAL
 * returned by pdGetPeripheral (ps2_sg_pad.c keeps the pad state host-side). */
static float core32_view[16];
static unsigned char core32_peripheral[0x34] __attribute__((aligned(4)));
unsigned int core32_view_buffer(void) { return (unsigned int)core32_view; }
unsigned int core32_peripheral_buffer(void) { return (unsigned int)core32_peripheral; }

/* game.c:354 bhDrawEnemy with cam.mtx as the current view (njSetMatrix(0, cam.mtx)). */
void core32_draw_enemy(void)
{
    const float* view = core32_view;
    int i;
    for (i = 0; i < 16; ++i)
        (*cam.mtx)[i] = view[i];
    njSetMatrix(NULL, cam.mtx);
    bhDrawEnemy();
}

typedef struct {
    unsigned int flg, id, type, stflg, mdflg;
    int hp, mode0, mode1, mode2, mode3;
    float px, py, pz;
    int ax, ay, az;
    unsigned int mtn_no, frm_no;
} CORE32_ENEMY_INFO;
static CORE32_ENEMY_INFO core32_info;

int core32_enemy_slots(void) { return sys->ewk_n; }

unsigned int core32_enemy_info(int i)
{
    BH_PWORK* ep = &ene[i];
    core32_info.flg = ep->flg; core32_info.id = ep->id; core32_info.type = ep->type;
    core32_info.stflg = ep->stflg; core32_info.mdflg = ep->mdflg;
    core32_info.hp = ep->hp;
    core32_info.mode0 = ep->mode0; core32_info.mode1 = ep->mode1;
    core32_info.mode2 = ep->mode2; core32_info.mode3 = ep->mode3;
    core32_info.px = ep->px; core32_info.py = ep->py; core32_info.pz = ep->pz;
    core32_info.ax = ep->ax; core32_info.ay = ep->ay; core32_info.az = ep->az;
    core32_info.mtn_no = ep->mtn_no; core32_info.frm_no = ep->frm_no;
    return (unsigned int)&core32_info;
}
