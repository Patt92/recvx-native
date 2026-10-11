#ifndef RECVX_NATIVE_EN11_H
#define RECVX_NATIVE_EN11_H
#include "enemy_work.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Dispatcher eneset.c:51: ID 11 = bhEne11. Originalkommentar en11.c:16:
   Spotter. Eigenstaendiger Game-Port, keine Originalkoerper/Generatoren.
   Die folgende Sicht enthaelt nur Eff261-Felder, keine zweite Effektwork. */
typedef struct {
    uint8_t mode0;
    int32_t ct2, ax, axp;
} RECVX_EN11_EFFECT;
typedef struct {
    uint16_t id;
    uint32_t flg;
    uint16_t type;
    uint8_t mdlver;
    RECVX_ENEMY_VEC pos, scale;
    int32_t ay;
} RECVX_EN11_EFFECT_TEMPLATE;

/* Fehlende Dienste des gemeinsamen Vertrags. Der Integrator bindet Anbieter
   des bestehenden Besitzers; NULL bedeutet benannte Portierungsluecke. */
typedef struct {
    void (*flags_clear)(uint32_t bank, uint32_t bits); /* rm_flg &= ~1 */
    int (*effect_create)(const RECVX_EN11_EFFECT_TEMPLATE*, RECVX_ENEMY_WORK* link);
    int (*effect_get)(int slot, RECVX_EN11_EFFECT* out);
    void (*effect_set)(int slot, const RECVX_EN11_EFFECT* in);
    int (*node_angles_get)(RECVX_ENEMY_WORK*, uint32_t node, int32_t out[3]);
    void (*node_angles_set)(RECVX_ENEMY_WORK*, uint32_t node, const int32_t in[3]);
    void (*node_work_flags)(RECVX_ENEMY_WORK*, uint32_t node, uint32_t set, uint32_t clear);
    /* zonzon1.c:852 RequestEnemySeBasic(enemy,pos,se,0,0), kein NoteOn. */
    void (*se_pan)(uint32_t enemy, const RECVX_ENEMY_VEC* pos, int se);
    /* bhEne03_CollisionBoxEdge (en03.c:5938) ist KEIN Dienst: EN11 ruft den
       gemeinsamen nativen EN03-Kollisionssatz (en03_collision.h) direkt. */
} RECVX_EN11_SERVICES;
void recvx_en11_bind(const RECVX_EN11_SERVICES* services);

/* exp0: 0x00..3f Orientierung; 0x56 Alarm, 0x58 ATR, 0x5c Richtung,
   0x60 Eff261. Hostzeiger werden durch stabile Besitzeridentitaeten ersetzt.
   Keine Lage/Motion/HP im EXP: diese gehoeren allein RECVX_ENEMY_WORK. */
typedef struct {
    float basis[16];
    uint8_t alarm;
    uint32_t wall_id;
    uint8_t wall_valid;          /* EXP0 0x58 != NULL */
    RECVX_ENEMY_ATR wall;
    int32_t dir;
    int effect_slot;
    uint8_t initialized;
} RECVX_EN11_STATE;
void recvx_en11(RECVX_ENEMY_WORK* w);
/* en11.c:1115 bhEne11_LightControl(epw, flg); Aufrufer ausserhalb von EN11:
   Ereignisbefehl Sub_controll 148 (event.c:9912-9923). 0 = Luecke gemeldet. */
int recvx_en11_light_control(RECVX_ENEMY_WORK* w, int mode);
#ifdef __cplusplus
}
#endif
#endif
