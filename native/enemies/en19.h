#ifndef RECVX_NATIVE_EN19_H
#define RECVX_NATIVE_EN19_H
#include "enemy_work.h"
#include "en03_lib.h"
#include "en19_tables.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Identitaet 19 — Tyrant T-078. Eigenstaendige native Zustandsmaschine auf dem
   WorldEntity-Work. Semantikreferenz src/ps2/veronica/prog/en19.c:226-4825 und
   include/ps2/veronica/prog/en19.h. Kein Originalkoerper, kein Generator,
   keine zweite Workwelt. Datentabellen in en19_tables.c. */

struct RECVX_EN19_STATE;
/* FW_WORK.prgP: void (*)(BH_PWORK*, en19_freework*, int count) (en19.h:137). */
typedef void (*RECVX_EN19_MV)(RECVX_ENEMY_WORK* w, struct RECVX_EN19_STATE* fw, int count);
/* PAW_WORK.p_prgP: void (*)(BH_PWORK* plP, void* paramP) mit paramP = exp0 (en19.c:4653). */
typedef void (*RECVX_EN19_PLY)(RECVX_ENEMY_PLAYER* p, RECVX_ENEMY_WORK* w, struct RECVX_EN19_STATE* fw);

/* en19.h:120 PAW_WORK (0x38): die Spieleraktion des Griffs/Schlags. ene_mnwP =
   ewP->mnwP ist nativ die Bibliothek des Gegnerworks (player_library Quelle 1). */
typedef struct {
    int32_t p_status, p_mode, p_act_flg;
    RECVX_EN19_PLY p_prgP;
    int32_t p_act_frm, p_chg_frm, p_mtn_rte, p_act_jmp, p_frm_num, p_act_now;
    const RECVX_EN19_ACT* act_tblP;
    int32_t tbl_num;
} RECVX_EN19_PAW;

/* en19.h:134 FW_WORK (0x204) als benannter exp0-Block (bhEne_CallocWork). */
typedef struct RECVX_EN19_STATE {
    float mtx_bak[16];                 /* 0x00 NJS_MATRIX                       */
    int32_t status, act_flg;           /* 0x40/0x44                             */
    RECVX_EN19_MV prgP;                /* 0x48                                  */
    int32_t act_frm, act_now, mtn_rte, chg_rte, act_cnt;      /* 0x4C..0x5C   */
    int32_t b_status, b_act_flg;       /* 0x60/0x64 (njMemCopy4 von status, 8 Worte) */
    RECVX_EN19_MV b_prgP;
    int32_t b_act_frm, b_act_now, b_mtn_rte, b_chg_rte, b_act_cnt;
    int32_t b_mtn_md, dummy;           /* 0x80/0x84                             */
    RECVX_EN19_PAW ply_act;            /* 0x88                                  */
    RECVX_ENEMY_WORK* ewP;             /* 0xC0                                  */
    int32_t br_mde0, br_mde1, br_mde2; /* 0xC4.. TY_BR_MODE0/1/2                */
    int32_t br_wait1, ct_dash, ct_pinch;
    RECVX_ENEMY_VEC ClwPos[4];         /* 0xDC                                  */
    RECVX_ENEMY_VEC fix_adj, adj_vx, adj_vz, tgt_pos;
    float ply_dst, tgt_dst;
    int32_t tgt_ang, trn_spd, dir_dlt, trw_dir;
    float trw_spd;
    int32_t ply_mde;
    RECVX_ENEMY_VEC wnd_spd;
    int32_t dmg_obj, dmg_cnt, dwn_cnt, bnd_tim, hp_bak, watr_top;
    float watr_rad;
    int32_t snd_no;
    float ovl_rte, atk_rng_a_near, atk_rng_b_near, atk_rng_a_far, atk_rng_b_far;
    int32_t dmg_flg, dmg_lvl, cmb_lvl, dmg_dly;  /* dmg_lvl: TY_DMG_MODE        */
    float pos_tmp[3];
    int32_t ang_tmp[3];
    RECVX_ENEMY_VEC vct_tmp;
    int32_t cnt_tmp;
    const RECVX_EN19_PMA* e0aP;        /* 0x1D4 Eff30aTbl[type] (3 Eintraege)    */
    int32_t eff_tim;
    /* R0_WK* clw0P/clw2P (0x1DC/0x1E0): die gespiegelten linken Klauen aus
       bhSetRapEff(312) (en19.c:4780 bhEne19_SetLeftClaw). Nativ Handles des
       Dienstes left_claw; -1 = NULL (Effekt nicht gebunden). */
    int32_t clw0, clw2;
    int32_t mode0_bak;                 /* 0x1E4 TY_PRG_MODE0                     */
    /* R0A_WORK* cam_clrP (0x1E8): nur in bhEne19 geschrieben, nie gelesen. */
    int32_t cam_clr;
    int32_t br_mde0_bak, br_mde1_bak, br_mde2_bak, comb_flg;
    /* --- nativ zusaetzlich, AUSSERHALB des Originalblocks (0x204): -----------
       EB_WORK.time (en19.h:224) liegt im Original im freien Bereich des
       Effekts 349 (en19.c:255-261, ebP->time = 0). Ohne eigenen Effektwork
       haelt der Zustand den Zaehler hier; gleiche Lebensdauer (Raum). */
    int32_t eb_time;
} RECVX_EN19_STATE;

/* sys->ef-Felder, die eine EN19-Routine VOR bhSetEffectTb schreibt. Die
   Vorlage ist persistent (effect.c sys->ef); nur maskierte Felder werden
   gesetzt, die uebrigen bleiben vom letzten Schreiber (z. B. bleibt sz in
   PlyDmg042 vom Vorgaenger, en19.c:4095; TyBloodSet schreibt sx und sz, nicht
   sy, en19.c:3645, Disassembly 0x1F52B8-0x1F52E0). */
#define RECVX_EN19_EF_FLG    0x001u
#define RECVX_EN19_EF_ID     0x002u
#define RECVX_EN19_EF_TYPE   0x004u
#define RECVX_EN19_EF_MDLVER 0x008u
#define RECVX_EN19_EF_FLR    0x010u
#define RECVX_EN19_EF_POS    0x020u
#define RECVX_EN19_EF_SX     0x040u
#define RECVX_EN19_EF_SY     0x080u
#define RECVX_EN19_EF_SZ     0x100u
#define RECVX_EN19_EF_AX     0x200u
#define RECVX_EN19_EF_AY     0x400u
typedef struct {
    uint32_t flg, id, type, mdlver;
    int32_t  flr_no;
    RECVX_ENEMY_VEC pos;
    float sx, sy, sz;
    int32_t ax, ay;
} RECVX_EN19_EF;

typedef struct RECVX_EN19_SERVICES {
    /* sys->gm_mode (en19.c:316 En19HpTbl[sys->gm_mode]). */
    int  (*game_mode)(int* out);
    /* sys->gm_flg (en19.c:239, Bit 0x1 sperrt die Effektanlage). */
    int  (*game_flags)(uint32_t* out);
    /* sys->rm_flg |= set; &= ~clear (en19.c:419, 1045-1082). */
    void (*room_flags)(uint32_t set, uint32_t clear);
    /* mlwP->objP[node].pos/.ang lesen bzw. einzeln schreiben (en19.c:363-392,
       448-453, 3063-3081, 3663-3667). */
    int  (*node_local)(const RECVX_ENEMY_WORK* w, uint32_t node, RECVX_ENEMY_VEC* pos, int32_t* ang3);
    void (*node_local_set)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, float v);
    void (*node_angle_set)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t axis, int32_t ang);
    /* mlwP->owP[node].flg = (flg & and) | or (en19.c:395-398, 533-534, 3084-3090). */
    int  (*owp_flags)(RECVX_ENEMY_WORK* w, uint32_t node, uint32_t and_flg, uint32_t or_flg);
    /* mnwP[mtn_no].atrP[frame] (en19.c:3809 bhEne19_MtnAttrbuteGet). Rueckgabe
       1 = bestimmt (*out = 0, wenn atrP NULL ist, wie im Original); 0 = der
       Frame liegt hinter der Tabelle (Original liest fremden Speicher). */
    int  (*motion_attribute)(const RECVX_ENEMY_WORK* w, uint32_t mtn_no, uint32_t frame, int32_t* out);
    /* Motion.c:868 bhCalcFixOffset(ewP, chain, NULL, out) (en19.c:3002). */
    int  (*fix_offset)(RECVX_ENEMY_WORK* w, const int8_t* chain, RECVX_ENEMY_VEC* out);
    /* hitchk.c:5474 bhCheckFloorEnemy(flr_no, px, pz) (en19.c:1060, 3876). */
    int  (*floor_enemy)(int flr_no, float px, float pz, RECVX_ENEMY_ATR* out, int32_t* index);
    /* rutchk.c:5 bhCheckRoute(ps0, ps1, ps2) -> Zone oder 0xFF; rutchk.c:47
       bhCheckRouteID(ps) (en19.c:1565-1578). */
    int  (*check_route)(const RECVX_ENEMY_VEC* from, const RECVX_ENEMY_VEC* to, RECVX_ENEMY_VEC* out);
    int  (*route_id)(const RECVX_ENEMY_VEC* pos);
    /* hitchk.c:2178 bhCheckWallRefAngle(pos, flg, ar, ah, &ay) (en19.c:4284):
       schreibt pos und *ay, 1 = Treffer. */
    int  (*wall_ref_angle)(RECVX_ENEMY_VEC* pos, uint32_t flg, float ar, float ah, int32_t* ay);
    /* hitchk.c:12 bhCheckWall(plp) mit plp->ah = 1.0f (en19.c:494-501): der
       Spielerwork gegen die Raumwaende mit ar/ah; schreibt p->pos. */
    int  (*player_wall)(RECVX_ENEMY_PLAYER* p, float ar, float ah);
    /* PEXP0_F(72) / PEXP0_F(80) (en19.c:503-504, macros.h:66). */
    void (*player_exp0_float_set)(uint32_t offset, float value);
    /* plp->mnwP: 1 = ewP->mnwP (en19.c:4694), 0 = plp->mnwPb (en19.c:4395). */
    void (*player_library)(const RECVX_ENEMY_WORK* w, uint32_t source);
    /* effect.c:436 bhSetEffectTb(&sys->ef, ofs, link, joint): maskierte Felder
       der persistenten Vorlage schreiben, dann anlegen. Slot oder -1. */
    int  (*effect_tb)(uint32_t mask, const RECVX_EN19_EF* ef, const RECVX_ENEMY_VEC* ofs,
                      RECVX_ENEMY_WORK* link, int joint);
    /* --- effsub3.c-Effekte (OR_WORK-Pool, bhSetRapEff): -----------------------
       Alle duerfen NULL sein; der Mechanismus meldet dann die Originalroutine. */
    /* effsub3.c:1640 rySetShadow(ewP, obj0, obj1, obj2, off_a, off_b) -> bhEff304. */
    int  (*shadow)(RECVX_ENEMY_WORK* w, int obj0, int obj1, int obj2, float off_a, float off_b);
    /* bhSetRapEff(310, PMA_WORK*, 8) -> bhEff30a Kamerawackel (en19.c:245, 2237, 4309). */
    int  (*cam_shake)(const RECVX_EN19_PMA* prm);
    /* bhSetRapEff(349, NULL, 0) mit fnc_prcP = bhEne19_TyBloodSet (en19.c:252-261):
       das Effektsystem ruft recvx_en19_ty_blood_set(w) je Effektpass. 1 = eingetragen. */
    int  (*blood_callback)(RECVX_ENEMY_WORK* w);
    /* bhSetRapEff(312, PRM_WRK, 9) -> bhEff30c (en19.c:4780 bhEne19_SetLeftClaw):
       gespiegeltes Modell von objP[obj] an owP[base]. Handle >= 0 oder -1. */
    int  (*left_claw)(RECVX_ENEMY_WORK* w, uint32_t base, uint32_t obj);
    /* R0_WK.mtxP des Handles (en19.c:2426/2581: fwP->clw0P->mtxP). */
    int  (*claw_matrix)(int handle, float* out16);
    /* bhSetRapEff(307, EFFPRM_WORK, 12) (en19.c:4801 bhEne19_SetClawPlane). */
    int  (*claw_plane)(RECVX_ENEMY_WORK* w, const float* mtx16, int col, int tim, float src_x, float dst_x);
    /* effsub3.c:2430 bhSetEffSpark(pos, dir, src_col, dst_col, typ_no) -> 308 + rom->lgtp[2]. */
    int  (*spark)(const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, uint32_t src_col, uint32_t dst_col, int typ_no);
    /* effsub3.c:2748 rySetEffBlood(mtxP, pos, dir, typ) -> 309; mtxP ist
       owP[joint].mtx des Links (Spieler oder Gegner). */
    int  (*blood)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, int typ_no);
    /* effsub3.c:3247 rySetEffBlood2(mtxP, pos, dir, typ) -> 311. */
    int  (*blood2)(RECVX_T3_LINK lk, int joint, const RECVX_ENEMY_VEC* pos, const RECVX_ENEMY_VEC* dir, int typ_no);
} RECVX_EN19_SERVICES;

void recvx_en19_bind(const RECVX_EN19_SERVICES* services);
/* Einstieg bhEne19 (en19.c:226) fuer bhJumpEnemy[19]. */
void recvx_en19(RECVX_ENEMY_WORK* w);
/* en19.c:3597 bhEne19_TyBloodSet: die Routine, die bhEff349 je Effektpass ueber
   fnc_prcP ruft (EB_WORK = {ewP, fwP, time}; time liegt im Zustand). */
void recvx_en19_ty_blood_set(RECVX_ENEMY_WORK* w);

#ifdef __cplusplus
}
#endif
#endif
