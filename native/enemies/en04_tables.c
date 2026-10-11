/* Reine Originaldaten: en04.c:26-409, :3391, :3957; en01.c:1345.
   Kein Funktionskoerper und keine generierte Logik. Duplikat MTN34 im
   SE-Satz ist absichtlich erhalten: beide Zeilen loesen aus. */
#include "en04_tables.h"
#define END {UINT32_MAX,0}
#define TWO(n,f,a,g,b) {n,{{f,a},{g,b},END,END}}
#define ONE(n,f,a) {n,{{f,a},END,END,END}}
const int32_t recvx_en04_player_mtn[4]={100,105,100,105};
const RECVX_EN04_MTBL recvx_en04_sfx[30]={
 TWO(0,18,0x12300,48,0x12300), TWO(1,0,0x12300,2,0x12300),
 TWO(2,0,0x12300,2,0x12300), TWO(3,0,0x12300,2,0x12300),
 TWO(34,0,0x12300,2,0x12300), TWO(40,0,0x12300,2,0x12300),
 TWO(41,0,0x12300,2,0x12300), TWO(4,4,0x12300,10,0x1011304),
 ONE(6,12,0x12301), ONE(7,9,0x1012302),
 TWO(8,12,0x1011306,22,0x12301), TWO(9,8,0x1011306,29,0x12301),
 TWO(14,0,0x11308,16,0x11308),
 {16,{{18,0x12301},{31,0x1011304},{55,0x1011306},{90,0x1012303}}},
 TWO(20,10,0x12301,18,0x12301),
 {21,{{0,0x11308},{12,0x1002309},{64,0x11308},END}},
 ONE(25,0,0x101230e), ONE(43,0,0x101230e), ONE(33,21,0x12301),
 TWO(34,0,0x12300,2,0x12300), ONE(35,9,0x12301),
 TWO(36,0,0x1011304,9,0x1011306), TWO(37,24,0x12301,37,0x12301),
 TWO(38,24,0x12301,41,0x12301), ONE(17,7,0x1011304),
 TWO(18,12,0x101130d,26,0x101130d), TWO(19,15,0x101130d,33,0x1011307),
 {46,{{27,0x12300},{42,0x12300},{54,0x12300},END}},
 TWO(48,20,0x12300,29,0x12300), {UINT32_MAX,{END,END,END,END}}
};
const RECVX_EN04_MTBL recvx_en04_vib[6]={
 ONE(17,12,0),TWO(18,11,0,25,0),TWO(19,10,0,36,1),
 TWO(16,0,1,70,0), {25,{{4,0},{22,0},{33,0},END}},
 {UINT32_MAX,{END,END,END,END}}
};
#undef ONE
#undef TWO
#undef END
const uint8_t recvx_en04_tree[8]={0,1,2,3,4,5,255,0};
const int8_t recvx_en04_shadow[6]={5,15,18,8,11,-1};
const RECVX_EN04_WPNDAMAGE recvx_en04_wpn_damage[22]={
 {0,0,0,0,0},{0,0,0,0,0},{0,0,3,3,3},{0,0,7,3,3},
 {0,0,7,3,3},{0,0,7,3,3},{0,0,5,3,5},{0,0,7,3,3},
 {0,0,7,3,3},{0,0,7,3,3},{0,0,7,3,3},{1,0,5,3,5},
 {0,0,7,3,3},{0,0,3,3,3},{2,0,5,3,5},{0x18,0,1,3,3},
 {0x0a,0,1,3,3},{0,0,1,3,3},{0x0c,0,5,3,5},{2,0,5,3,5},
 {0x0c,0,5,3,5},{0,0,1,3,3}
};
const RECVX_EN04_COMBWEP recvx_en04_combo_wep[21]={{0,{0,0,0},0,0}};
const RECVX_EN04_COMBJOINT recvx_en04_combo_joint[19]={{0,0}};
const RECVX_ENEMY_CPCL recvx_en04_capsules[17]={
 {1,2,13},{2,3,13},{4,4,14},{0,4,-6},{5,5,12},{0,2,-8},
 {12,12,15},{0,0,10},{6,7,8},{7,8,4},{9,10,8},{10,11,4},
 {13,14,8},{14,15,4},{16,17,8},{17,18,4},{0,0,0}
};
const RECVX_ENEMY_BP_WORK recvx_en04_blood_pool={
 {0,0.1f,0},0,0,0.08f,{0.8f,0.6f,0,0.8f,0.9f},{0,4,8,12,14}
};
const RECVX_EN04_BT recvx_en04_blood[19]={
 {0,0,0,0,0,0,0,0}, {1,0,0,0,1,1,3,1}, {2,0,0,0,1,1,4,1},
 {3,0,0,0,.5f,.5f,3,1}, {4,0,0,0,.5f,.5f,2,1},
 {5,0,0,0,.5f,.5f,3,1}, {6,0,0,0,.2f,.5f,4,1},
 {7,0,0,0,.2f,.5f,2,1}, {8,0,0,0,.2f,.2f,3,1},
 {9,0,0,0,.2f,.5f,4,1}, {10,0,0,0,.2f,.5f,2,1},
 {11,0,0,0,.2f,.2f,3,1},
 /* en04.c:378: Zeile 12 fuehrt xlen/ylen 0 und size 2, nicht .2/.5/4. */
 {12,0,0,0,0,0,2,1},
 {13,0,0,0,.2f,.5f,4,1}, {14,0,0,0,.2f,.5f,2,1},
 {15,0,0,0,.2f,.2f,3,1}, {16,0,0,0,.2f,.5f,4,1},
 {17,0,0,0,.2f,.5f,2,1}, {18,0,0,0,.2f,.2f,3,1}
};
const uint8_t recvx_en04_flip[20]={0,1,2,3,4,5,9,10,11,6,7,8,12,16,17,18,13,14,15,255};
const RECVX_ENEMY_VEC recvx_en04_player_offset[4]={
 {-.937615f,0,-9.776372f},{-.112207f,0,-10.525611f},
 {-.937615f,0,-9.776372f},{-.112207f,0,-10.525611f}
};
const int32_t recvx_en04_stair_angle[4]={0,49152,32768,16384};
const RECVX_ENEMY_VEC recvx_en04_wire_offset[10]={
 {0,.1f,.05f},{0,-.1f,.1f},{0,.1f,.35f},{0,-.1f,0},{0,.1f,0},
 {0,-.1f,-.2f},{0,.1f,-.15f},{0,-.1f,-.1f},{0,.1f,-.05f},{0,-.1f,0}
};

/* en04.c:390 en04_hp_tbl */
const int recvx_en04_hp_tbl[2][16]={
 { 60, 60, 60, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 120, 120, 120 },
 { 30, 30, 30, 45, 45, 45, 45, 45, 60, 60, 60, 60, 60,  75,  75,  75 }
};

const RECVX_EN04_DATA* recvx_en04_default_data(void){
 static const RECVX_EN04_DATA s_data = {
  recvx_en04_hp_tbl
 };
 return &s_data;
}
