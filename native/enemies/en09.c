/* Identitaet 9 — Bandersnatch. Nativ eigenstaendig bestimmt; kein Original-
   koerper, kein Generator, keine zweite Workwelt. Fundstellen stehen in
   en09.h und zusaetzlich an jeder Funktion.

   Init/Motionschicht und die folgenden Zustandsketten werden auf den
   bestehenden World-/Assetbesitzern portiert. Nicht portierte Zweige melden
   weiterhin ihren Originalnamen; keine neutrale Ersatzantwort.
 */
#include "en09.h"
#include "en09_tables.h"
#include "en03_lib.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../ninja_math.h"
#include "../ninja_matrix.h"
#include "../combat_geometry.h"

/* ---------------------------------------------------------------------------
   Lage der exp0-Felder gegen die Originaloffsets (en09.c:2376: 0x7C Byte)
   --------------------------------------------------------------------------- */
_Static_assert(offsetof(RECVX_EN09_EXT, arm_len)     == 0x00u, "EXP0_F(0x0..0xC)");
_Static_assert(offsetof(RECVX_EN09_EXT, search)      == 0x10u, "EXP0_UC(0x10)");
_Static_assert(offsetof(RECVX_EN09_EXT, sight_hold)  == 0x11u, "EXP0_C(0x11)");
_Static_assert(offsetof(RECVX_EN09_EXT, shape_count) == 0x13u, "EXP0_UC(0x13)");
_Static_assert(offsetof(RECVX_EN09_EXT, ct_14)       == 0x14u, "EXP0_I(0x14)");
_Static_assert(offsetof(RECVX_EN09_EXT, state)       == 0x18u, "EXP0_I(0x18)");
_Static_assert(offsetof(RECVX_EN09_EXT, ct_1c)       == 0x1Cu, "EXP0_I(0x1C)");
_Static_assert(offsetof(RECVX_EN09_EXT, ct_20)       == 0x20u, "EXP0_I(0x20)");
_Static_assert(offsetof(RECVX_EN09_EXT, ply_dist)    == 0x28u, "EXP0_F(0x28)");
_Static_assert(offsetof(RECVX_EN09_EXT, trg_x)       == 0x2Cu, "EXP0_F(0x2C)");
_Static_assert(offsetof(RECVX_EN09_EXT, trg_z)       == 0x34u, "EXP0_F(0x34)");
_Static_assert(offsetof(RECVX_EN09_EXT, trg2_x)      == 0x44u, "EXP0_F(0x44)");
_Static_assert(offsetof(RECVX_EN09_EXT, trg2_z)      == 0x4Cu, "EXP0_F(0x4C)");
_Static_assert(offsetof(RECVX_EN09_EXT, wall_atr)    == 0x50u, "exp0 + 0x50 (ATR_WORK*)");
_Static_assert(offsetof(RECVX_EN09_EXT, route_atr)   == 0x54u, "exp0 + 0x54 (ATR_WORK*)");
_Static_assert(offsetof(RECVX_EN09_EXT, arm_len_out) == 0x6Cu, "EXP0_F(0x6C)");
_Static_assert(offsetof(RECVX_EN09_EXT, wall_rec)    == 0x7Cu,
               "nativer Zusatz liegt hinter dem Originalblock");

static const RECVX_EN09_SERVICES* en09_services;
void recvx_en09_bind(const RECVX_EN09_SERVICES* s){en09_services=s;}

#define EN09_EXP0_SIZE 0x7Cu   /* en09.c:2376 bhEne_CallocWork(0x7C, 8) */

/* ---------------------------------------------------------------------------
   Zugriffshelfer
   --------------------------------------------------------------------------- */
static void en09_gap(RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
    else                          recvx_t3_gap(name);
}

static RECVX_EN09_EXT* en09_ext(RECVX_ENEMY_WORK* w)
{
    return w ? (RECVX_EN09_EXT*)w->exp0 : NULL;
}

static const RECVX_EN09_DATA* en09_tables(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN09_DATA* d = recvx_en09_data();
    if (!d) en09_gap(w, "en09: Datensatz nicht gebunden (recvx_en09_set_data)");
    return d;
}

/* en09.c:7694 bhEne09_SePlay — RequestEnemySe(sys->enow, &epw->px, no) mit dem
   Originalgate flg 0x10000. Gleiche Bedingung wie bhEne_CallSE
   (zonzon1.c:828), aber als eigene Routine im Original. */
static void en09_se_play(RECVX_ENEMY_WORK* w, int no)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC pos;

    if (w->flg & 0x10000u) return;
    if (!sv || !sv->enemy_se || !sv->enemy_now) { en09_gap(w, "RequestEnemySe"); return; }

    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    sv->enemy_se(sv->enemy_now(), &pos, no);
}

/* en09.c:7369 bhEne09_ChkDiffAngle — vorzeichenbehaftete Winkeldifferenz.
   Die Originalform `(ang - 0x8000) - 0x8000` ist dieselbe Rechnung wie
   `ang - 0x10000`; sie bleibt wortgleich stehen. */
int recvx_en09_chk_diff_angle(int pang, int dang)
{
    int ang = (dang - pang) & 0xFFFF;

    if (ang > 0x8000) ang = (ang - 0x8000) - 0x8000;

    return ang;
}

/* ---------------------------------------------------------------------------
   en09.c:2128 bhEne09_CalcEnemy
   ---------------------------------------------------------------------------
   bhCalcModel, dann Koerperhoehe aus Knoten 4, Vorwaertsversatz aus der
   Wurzelmatrix und die Waffenkapsel aus den Knoten 4, 20 und 23.
   --------------------------------------------------------------------------- */
static void en09_calc_enemy(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC local, world, j4, j20, j23;
    float mtx[16];

    if (!sv || !sv->calc_model) { en09_gap(w, "bhCalcModel"); return; }
    sv->calc_model(w);

    if (!sv->joint_matrix || !sv->joint_point) { en09_gap(w, "mlwP->owP[].mtx"); return; }

    /* :2136 epw->cah = epw->ah = owP[4].mtx[13] - epw->py */
    if (!sv->joint_matrix(w, 4u, mtx)) { en09_gap(w, "mlwP->owP[4]"); return; }
    j4.x = mtx[12]; j4.y = mtx[13]; j4.z = mtx[14];
    w->ah = w->cah = j4.y - w->py;

    /* :2139-2146 njCalcPoint(owP[0].mtx, {2,0,0}) -> aox/aoz */
    local.x = 2.0f; local.y = 0.0f; local.z = 0.0f;
    if (!sv->joint_point(w, 0u, &local, &world)) { en09_gap(w, "mlwP->owP[0]"); return; }
    w->aox = world.x - w->px;
    w->aoz = world.z - w->pz;

    /* :2149-2163 watr: c1 = Knoten 4, c2 = Mitte zwischen Knoten 20 und 23. */
    if (!sv->joint_matrix(w, 20u, mtx)) { en09_gap(w, "mlwP->owP[20]"); return; }
    j20.x = mtx[12]; j20.y = mtx[13]; j20.z = mtx[14];
    if (!sv->joint_matrix(w, 23u, mtx)) { en09_gap(w, "mlwP->owP[23]"); return; }
    j23.x = mtx[12]; j23.y = mtx[13]; j23.z = mtx[14];

    w->watr.c1 = j4;
    w->watr.c2.x = (j20.x + j23.x) / 2.0f;
    w->watr.c2.y = (j20.y + j23.y) / 2.0f;
    w->watr.c2.z = (j20.z + j23.z) / 2.0f;
    w->watr.r = 2.5f;
}

/* en09.c:2326 bhEne09_CollCheckArm — im Original eine LEERE Funktion. Sie
   wird an vier Stellen von CollCheckWall gerufen (:2221/:2262/:2295/:2320).
   Der leere Koerper ist Originalverhalten, keine Luecke. */
static void en09_coll_check_arm(RECVX_ENEMY_WORK* w) { (void)w; }

/* ---------------------------------------------------------------------------
   en09.c:2186 bhEne09_CollCheckWall
   --------------------------------------------------------------------------- */
static void en09_coll_check_wall(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN09_EXT* e = en09_ext(w);
    const RECVX_EN09_DATA* d;
    RECVX_ENEMY_VEC ps, ops, pd;
    float mtx[16];
    /* Das Original gibt `lcmat` mit, einen gemeinsamen Streichmatrixpuffer
       (en09.c:2239/2240/2280/2299). Nativ ist das ein lokaler Puffer; die
       Routine schreibt nur hinein und liest ihn nicht zwischen den Aufrufen. */
    float lcmat[16];
    uint32_t mode;

    if (!e) { en09_gap(w, "bhEne09_CollCheckWall:exp0 fehlt"); return; }
    if ((e->state & RECVX_EN09_ST_NO_WALL) || !(w->flg & 0x10u)) return;

    d = en09_tables(w);
    if (!d) return;

    w->stflg &= ~1u;                                      /* :2196 */

    ps.x=w->px+w->aox;ps.y=w->py+w->aoy;ps.z=w->pz+w->aoz;
    if(!en09_services||!en09_services->wall){en09_gap(w,"bhCheckWallType: Datensatzanbieter fehlt");return;}
    e->wall_rec_valid=en09_services->wall(&ps,w->flg,w->ar,w->ah,&e->wall_rec);
    e->wall_atr=e->wall_rec_valid?1u:0u;

    mode = e->state & RECVX_EN09_ST_COLL_CASE;            /* :2205 ff. */

    if (mode == 1u || mode == 2u || mode == 3u) {
        /* Allen drei Faellen gemeinsam: Koerperschritt gegen die Waende. */
        ps.x  = w->px  + w->aox; ps.y  = w->py  + w->aoy; ps.z  = w->pz  + w->aoz;
        ops.x = w->pxb + w->aox; ops.y = w->pyb + w->aoy; ops.z = w->pzb + w->aoz;
        pd.x = pd.y = pd.z = 0.0f;
        (void)recvx_t3_collision_check_wall2(w, &ps, &ops, &pd, w->ar, w->ah);
        w->px += pd.x; w->py += pd.y; w->pz += pd.z;

        if (mode == 2u) {
            /* :2239-2256 Mitte zwischen beiden Beinketten gegen die Mitte
               zwischen den Knoten 23 und 20, Radius 2.0. */
            pd.x = pd.y = pd.z = 0.0f;
            (void)recvx_t3_calc_parts_pos(w, lcmat, &pd, (const uint8_t*)d->tree[0], 5, 1);
            (void)recvx_t3_calc_parts_pos(w, lcmat, &ps, (const uint8_t*)d->tree[1], 5, 1);
            ps.x = (pd.x + ps.x) / 2.0f;
            ps.y = (pd.y + ps.y) / 2.0f;
            ps.z = (pd.z + ps.z) / 2.0f;

            if (sv && sv->joint_matrix && sv->joint_matrix(w, 23u, mtx)) {
                ops.x = mtx[12]; ops.y = mtx[13]; ops.z = mtx[14];
                if (sv->joint_matrix(w, 20u, mtx)) {
                    ops.x = (ops.x + mtx[12]) / 2.0f;
                    ops.y = (ops.y + mtx[13]) / 2.0f;
                    ops.z = (ops.z + mtx[14]) / 2.0f;
                    pd.x = pd.y = pd.z = 0.0f;
                    (void)recvx_t3_collision_check_wall2(w, &ps, &ops, &pd, 2.0f, w->ah);
                    w->px += pd.x; w->py += pd.y; w->pz += pd.z;
                } else en09_gap(w, "mlwP->owP[20]");
            } else en09_gap(w, "mlwP->owP[23]");
        } else if (mode == 3u) {
            /* :2280-2293 Armkette gegen Knoten 4.
               ORIGINALER QUIRK (:2286): `ops.y = epw->pxb` — dort steht die
               vorherige X-Lage in der Y-Komponente. Unveraendert uebernommen. */
            (void)recvx_t3_calc_parts_pos(w, lcmat, &ps, (const uint8_t*)d->tree[3], 5, 1);
            ps.y = w->py;

            if (sv && sv->joint_matrix && sv->joint_matrix(w, 4u, mtx)) {
                ops.x = mtx[12];
                ops.y = w->pxb;
                ops.z = mtx[14];
                pd.x = pd.y = pd.z = 0.0f;
                (void)recvx_t3_collision_check_wall2(w, &ps, &ops, &pd, 2.0f, w->ah);
                w->px += pd.x; w->py += pd.y; w->pz += pd.z;
            } else en09_gap(w, "mlwP->owP[4]");
        }
        en09_coll_check_arm(w);
    } else if (mode == 6u) {
        /* :2299-2320 Rumpfkette (zwei Glieder) gegen Knoten 1, Radius 4.0. */
        (void)recvx_t3_calc_parts_pos(w, lcmat, &ps, (const uint8_t*)d->tree[2], 2, 1);

        if (sv && sv->joint_matrix && sv->joint_matrix(w, 1u, mtx)) {
            ops.x = mtx[12]; ops.y = mtx[13]; ops.z = mtx[14];
            ps.x  += w->aox; ps.y  += w->aoy; ps.z  += w->aoz;
            ops.x += w->aox; ops.y += w->aoy; ops.z += w->aoz;
            pd.x = pd.y = pd.z = 0.0f;
            (void)recvx_t3_collision_check_wall2(w, &ps, &ops, &pd, 4.0f, w->ah);
            w->px += pd.x; w->py += pd.y; w->pz += pd.z;
            en09_coll_check_arm(w);
        } else en09_gap(w, "mlwP->owP[1]");
    }
}

/* en09.c:2167 bhEne09_CollCheck */
static void en09_coll_check(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN09_EXT* e = en09_ext(w);

    if (!(w->flg & 2u)) {
        if (!e) { en09_gap(w, "bhEne09_CollCheck:exp0 fehlt"); return; }
        if (!(e->state & RECVX_EN09_ST_NO_PLAYER)) {
            if (sv && sv->check_player) (void)sv->check_player(w);
            else                        en09_gap(w, "bhCheckPlayer");
        }
        if (!(e->state & RECVX_EN09_ST_NO_ENEMIES)) {
            if (sv && sv->check_enemies) (void)sv->check_enemies(w);
            else                         en09_gap(w, "bhCheckEnemies");
        }
    }
    en09_coll_check_wall(w);
}

/* ---------------------------------------------------------------------------
   en09.c:7558 bhEne09_MtnTblPlay
   ---------------------------------------------------------------------------
   Laeuft NUR, solange die Motion NICHT aus der Raumliste kommt
   (sys->rmthp != epw->mnwP, also mtn_room == 0).
   --------------------------------------------------------------------------- */
static void en09_mtn_tbl_play(RECVX_ENEMY_WORK* w, int frm)
{
    RECVX_EN09_EXT* e = en09_ext(w);
    const RECVX_EN09_DATA* d = en09_tables(w);
    const RECVX_EN09_MTN_FMTN* mtbl;
    const RECVX_EN09_MTN_ATB*  mtbl2;
    const RECVX_EN09_MTN_ATB*  mtbl3;
    RECVX_ENEMY_VEC vec2 = { 0.0f, 0.0f, 0.0f };
    int i;

    if (!e || !d) return;
    if (w->mtn_room) return;                              /* :7572 */

    if (w->flg & 0x40000u) {                              /* :7574 */
        for (mtbl = d->mtn_tbl; mtbl->no != -1; ++mtbl) {
            if (mtbl->no != (int32_t)w->mtn_no) continue;
            for (i = 0; i < 3; ++i) {
                if (mtbl->fmtn[i].type == -1) continue;
                if (frm < mtbl->fmtn[i].s_frm || frm > mtbl->fmtn[i].e_frm) continue;

                /* :7586-7636. Die Typen 7 und 8 setzen zuerst den Versatz
                   z = -1.5 und fallen dann in 0 bzw. 1 durch; 10000000 des
                   Zustandswortes vertauscht die beiden Beinketten. */
                switch (mtbl->fmtn[i].type) {
                case 7:
                    vec2.x = 0.0f; vec2.y = 0.0f; vec2.z = -1.5f;
                    /* fall through — ORIGINALER DURCHFALL (en09.c:7593) */
                case 0:
                    if(en09_services&&en09_services->fix_offset){RECVX_ENEMY_VEC v={0};
                        en09_services->fix_offset(w,(const int8_t*)d->tree[(e->state&0x10000000u)?1:0],&vec2,&v);
                        w->px-=v.x;w->pz-=v.z;}else en09_gap(w,"bhCalcFixOffset");
                    break;
                case 8:
                    vec2.x = 0.0f; vec2.y = 0.0f; vec2.z = -1.5f;
                    /* fall through — ORIGINALER DURCHFALL (en09.c:7614) */
                case 1:
                    if(en09_services&&en09_services->fix_offset){RECVX_ENEMY_VEC v={0};
                        en09_services->fix_offset(w,(const int8_t*)d->tree[(e->state&0x10000000u)?0:1],&vec2,&v);
                        w->px-=v.x;w->pz-=v.z;}else en09_gap(w,"bhCalcFixOffset");
                    break;
                case 6:
                    if(en09_services&&en09_services->fix_offset){RECVX_ENEMY_VEC v={0};
                        en09_services->fix_offset(w,(const int8_t*)d->tree[2],NULL,&v);
                        w->px-=v.x;w->py-=v.y;w->pz-=v.z;}else en09_gap(w,"bhCalcFixOffset");
                    break;
                default:
                    break;
                }
                break;   /* :7638 — nur der erste passende Abschnitt */
            }
        }
    }

    for (mtbl2 = d->mtn_tbl2; mtbl2->no != -1; ++mtbl2) {  /* :7648 */
        if (mtbl2->no != (int32_t)w->mtn_no) continue;
        for (i = 0; i < 5; ++i) {
            if (mtbl2->atb[i].frm == -1) break;
            if (mtbl2->atb[i].frm == frm) en09_se_play(w, (int)mtbl2->atb[i].act);
        }
    }

    if (w->mode0 < 5u) {                                   /* :7668 */
        for (mtbl3 = d->mtn_tbl3; mtbl3->no != -1; ++mtbl3) {
            if (mtbl3->no != (int32_t)w->mtn_no) continue;
            for (i = 0; i < 5; ++i) {
                if (mtbl3->atb[i].frm == -1) break;
                if (mtbl3->atb[i].frm == frm) recvx_t3_set_vibration((int)mtbl3->atb[i].act);
            }
        }
    }
}

/* ---------------------------------------------------------------------------
   en09.c:7396 bhEne09_SetMtn
   --------------------------------------------------------------------------- */
static int en09_set_mtn(RECVX_ENEMY_WORK* w)
{
    /* :7398/:7405 — zwei lokale Felder im Original, hier unveraendert. */
    static const int32_t rot_tbl[4]  = { 0x00000800, 0x00001600, 0x00001800, 0x00001800 };
    static const int32_t rot_tbl2[4] = { -2048,      -5632,      -6144,      -6144      };

    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN09_EXT* e = en09_ext(w);
    const RECVX_EN09_DATA* d = en09_tables(w);
    const RECVX_EN09_ARM_POS* arm;
    float arm_len = 0.0f;
    int ret, frm;

    if (!e || !d) return 0;

    frm = w->frm_no >> 16;                                 /* :7418 */

    if (!sv || !sv->set_motion) { en09_gap(w, "bhSetMotion"); return 0; }
    ret = sv->set_motion(w, w->mtn_add, w->mtn_md);         /* :7420 */

    if (ret != 0) w->flg |=  0x02000000u;                   /* :7424 */
    else          w->flg &= ~0x02000000u;                   /* :7428 */

    /* :7431 Armlaenge nur bei Raum-Motion und mtn_no 2..5. */
    if (w->mtn_room && (w->mtn_no == 2u || (w->mtn_no - 3u) < 3u)) {
        if      (w->mtn_no == 2u) arm = d->arm_pos3;
        else if (w->mtn_no == 3u) arm = d->arm_pos4;
        else if (w->mtn_no == 4u) arm = d->arm_pos5;
        else                      arm = d->arm_pos6;

        /* :7452-7458. Der Originalaufbau setzt arm_len VOR dem Abbruchtest;
           beim ersten Eintrag mit frm >= frm gilt daher DIESER Eintrag. */
        while (arm->frm != -1) {
            arm_len = arm->len;
            if (arm->frm < frm) ++arm;
            else break;
        }

        /* :7460-7473. Die vier Armsegmente liegen an den Knoten 7..10; das
           Original adressiert sie als objP[7].pos[0/13/26/39], weil ein
           NJS_CNK_OBJECT dreizehn Worte breit ist. Nativ sind es dieselben
           vier Knoten, Achse 0. Der schreibende Knotendienst fehlt im
           gemeinsamen Satz (siehe Dateikopf, Punkt 1). */
        if(!en09_services||!en09_services->node||!en09_services->node_pos){en09_gap(w,"bhEne09_SetMtn: Armknoten");return ret;}
        for(int k=0;k<4;++k){RECVX_ENEMY_VEC p,last;
            if(!en09_services->node(w,7u+k,&p,NULL)||!en09_services->node(w,10,&last,NULL)){en09_gap(w,"bhEne09_SetMtn: Armknoten fehlt");return ret;}
            en09_services->node_pos(w,7u+k,0,frm==0?e->arm_len[k]:p.x+(arm_len-last.x)/4.0f);}
    }

    /* :7476-7501 Wurzelversatz der Motion. */
    if (e->state & RECVX_EN09_ST_ROOT_MTN_3) {
        if(en09_services&&en09_services->node_pos){en09_services->node_pos(w,0,0,0);en09_services->node_pos(w,0,2,0);
            if(e->state&RECVX_EN09_ST_ROOT_MTN_3)en09_services->node_pos(w,0,1,0);}
        else {en09_gap(w,"bhEne09_SetMtn: Wurzelknoten");return ret;}
        recvx_t3_get_translate_mtn(w, frm, 1);
    } else if (e->state & RECVX_EN09_ST_ROOT_MTN_XZ) {
        if(en09_services&&en09_services->node_pos){en09_services->node_pos(w,0,0,0);en09_services->node_pos(w,0,2,0);
            if(e->state&RECVX_EN09_ST_ROOT_MTN_3)en09_services->node_pos(w,0,1,0);}
        else {en09_gap(w,"bhEne09_SetMtn: Wurzelknoten");return ret;}
        recvx_t3_get_translate_mtn(w, frm, 0);
    } else if (e->state & RECVX_EN09_ST_ROOT_XZ) {
        if(en09_services&&en09_services->node_pos){en09_services->node_pos(w,0,0,0);en09_services->node_pos(w,0,2,0);
            if(e->state&RECVX_EN09_ST_ROOT_MTN_3)en09_services->node_pos(w,0,1,0);}
        else {en09_gap(w,"bhEne09_SetMtn: Wurzelknoten");return ret;}
    }

    w->mdflg |= 2u;                                         /* :7503 */

    /* :7505 shp_ct aus der Formkurve; der Zaehler ist ein Byte und laeuft
       daher von selbst um (& 0xFF ist im Original zusaetzlich notiert). */
    e->shape_count = (uint8_t)(e->shape_count + 1u);
    w->shp_ct = d->mogmog[(e->shape_count & 0xFFu) % RECVX_EN09_MOGMOG_COUNT];

    /* :7507-7542 Kopfdrehung ueber Knoten 2. */
    if (e->state & RECVX_EN09_ST_HEAD_ROT) {
        int32_t ang[3];
        if(!en09_services||!en09_services->node||!en09_services->node_ang||!en09_services->node(w,2,NULL,ang)){en09_gap(w,"bhEne09_SetMtn: Kopfknoten");return ret;}
        if(w->mtn_no==0){ang[0]+=32768;ang[1]=32768-ang[1];ang[2]+=32768;
            for(int k=0;k<3;++k)en09_services->node_ang(w,2,k,ang[k]);}

        if (w->ct3 < 4) {
            const int32_t step = (e->state & RECVX_EN09_ST_ROT_UP)
                               ? rot_tbl[w->ct3] : rot_tbl2[w->ct3];
            en09_services->node_ang(w,2,0,ang[0]+step);
            if (w->ct3 == 3) w->hokan_count = 7u;
        } else if (w->ct3 >= 9) {
            e->state &= ~RECVX_EN09_ST_HEAD_ROT;
        }
        w->ct3++;
    }

    /* :7544-7550 */
    if (w->mode0 < 5u && w->mtn_no == 0xFu && frm < 0xC){
        if(en09_services&&en09_services->node_pos)en09_services->node_pos(w,0,1,0);
        else en09_gap(w,"bhEne09_SetMtn: Wurzelknoten");
    }
    en09_mtn_tbl_play(w, frm);                              /* :7552 */

    return ret;
}

/* ---------------------------------------------------------------------------
   en09.c:2332 bhEne09_Init
   --------------------------------------------------------------------------- */
static void en09_init(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    const RECVX_EN09_DATA* d = en09_tables(w);
    RECVX_EN09_EXT* e;
    float save[4];
    int i;

    if (!d) return;

    w->ar  = 5.0f;  w->ah  = 18.0f;                        /* :2341 */
    w->aw  = 0.0f;  w->ad  = 0.0f;
    w->car = 4.0f;  w->cah = 18.0f;
    w->stflg = 0u;
    w->aox = w->aoy = w->aoz = 0.0f;

    if(!en09_services||!en09_services->game_mode||!en09_services->node||!en09_services->node_pos||!en09_services->model_init){
        en09_gap(w,"bhEne09_Init: World-/Modellanbieter fehlt");return;}
    w->hp=d->hp_tbl[en09_services->game_mode()==2][recvx_t3_rand_mod(16)];

    for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;   /* :2361 */

    w->hokan_rate  = 0;                                    /* :2366 */
    w->hokan_count = 0u;
    w->mtn_no  = 1u;
    w->mtn_add = 0;
    w->frm_no  = 0;
    w->mtn_tp  = NULL;
    w->mtn_md  = 0x20u;

    if (w->exp0 == NULL) {                                 /* :2374 */
        if (!sv || !sv->calloc_work) { en09_gap(w, "bhEne_CallocWork"); return; }
        w->exp0 = sv->calloc_work((uint32_t)sizeof(RECVX_EN09_EXT));
        if (!w->exp0) { en09_gap(w, "bhEne_CallocWork: exp-Budget erschoepft"); return; }
        e = en09_ext(w);
        /* :2378-2382 Startlaengen der vier Armsegmente aus objP[7..10]. */
        for(i=0;i<4;++i){RECVX_ENEMY_VEC p;
            if(!en09_services->node(w,7u+i,&p,NULL)){en09_gap(w,"bhEne09_Init: Armknoten fehlt");return;}
            e->arm_len[i]=p.x;}
    } else {                                               /* :2384 */
        e = en09_ext(w);
        if (e->state & RECVX_EN09_ST_INIT_PLY){
            if(!en09_services->player_node_flags_clear){en09_gap(w,"plp->owP[4/5].flg");return;}
            en09_services->player_node_flags_clear(4,3);en09_services->player_node_flags_clear(5,2);}

        for (i = 0; i < 4; ++i) save[i] = e->arm_len[i];   /* :2395 */

        /* :2400-2405 — der Originalblock wird von Hand genullt. Der native
           Block ist groesser (Flaechendatensatz dahinter); genullt wird
           genau derselbe Umfang wie im Original, danach stehen die geretteten
           Laengen wieder. */
        {
            unsigned char* p = (unsigned char*)w->exp0;
            uint32_t n = EN09_EXP0_SIZE;
            while (n-- != 0u) *p++ = 0u;
        }
        for (i = 0; i < 4; ++i) e->arm_len[i] = save[i];   /* :2407 */
        e->wall_atr = 0u;  e->wall_rec_valid  = 0;
        e->route_atr = 0u; e->route_rec_valid = 0;

        /* :2412-2416 die geretteten Laengen zurueck in die Knoten 7..10. */
        for(i=0;i<4;++i)en09_services->node_pos(w,7u+i,0,e->arm_len[i]);
    }

    e = en09_ext(w);
    if (!e) return;

    e->state &= ~RECVX_EN09_ST_COLL_CASE;                  /* :2419 */
    e->state |= 1u;                                        /* :2420 */

    w->flg |= 0x178u;                                      /* :2422 */
    w->flg &= ~2u;

    en09_services->model_init(w);
    w->mdflg = 0u;                                         /* :2425 */
    w->cpcl = d->cap_col;                                  /* :2428 */

    if (!(w->flg & 0x800u)) {                              /* :2430 */
        if (sv && sv->set_shadow) {
            int slot = sv->set_shadow(d->sdw_tab, w, 1, 5.0f, 4.0f, 4.0f);
            if (slot < 0) en09_gap(w, "bhSetShadow: eff-Pool voll");
        } else en09_gap(w, "bhSetShadow");
        w->flg |= 0x800u;
    }

    w->mdflg &= ~0x400u;                                   /* :2436 */

    /* Materialfarbe wird vom selben model_init-Anbieter gesetzt (:2438). */

    for (i = 0; i < 8; ++i) w->clp_jno[i] = recvx_en09_clp_jno[i];  /* :2440 */
    w->mode0 = 1u; w->mode1 = 0u; w->mode2 = 0u; w->mode3 = 0u;     /* :2448 */
    w->lok_jno = 4;                                        /* :2452 */

    if (sv && sv->set_motion) (void)sv->set_motion(w, w->mtn_add, w->mtn_md); /* :2454 */
    else                      en09_gap(w, "bhSetMotion");

    en09_calc_enemy(w);                                    /* :2456 */
}

/* ---------------------------------------------------------------------------
   Zustandsverteiler. Die Koerper sind noch nicht portiert; jeder meldet
   seinen Originalnamen und tut nichts (en09.c:1915-2014 sind die Tabellen).
   --------------------------------------------------------------------------- */
#include "en09_behavior.inc"

static void en09_move(RECVX_ENEMY_WORK* w)
{
    /* :2460 bhEne09_MoveType[epw->type]; 0..8 MVType00, 9 MVType09, 10 MVType10. */
    if (w->type >= RECVX_EN09_MOVE_TYPE_COUNT) {
        en09_gap(w, "bhEne09_MoveType: type ausserhalb der Tabelle");
        return;
    }
    RECVX_ENEMY_PLAYER plp;if(!en09_player(w,&plp))return;
    en09_ext(w)->ply_dist=en09_distance(plp.pos,en09_pos(w));
    if(getenv("EN09_MOVE_DBG"))fprintf(stderr,"EN09MOVE id=%u type=%u mode=%u,%u,%u flr=%d px=%.2f pz=%.2f\n",w->id,w->type,w->mode0,w->mode1,w->mode2,w->flr_no,w->px,w->pz);
    if(w->type==9){en09_ext(w)->trg_x=w->px;en09_ext(w)->trg_z=w->pz;en09_transition(w,10);en09_move_state(w,&plp);}
    else if(w->type==10){if(w->mode2==3)en09_mv03(w);else en09_mv00(w);}
    else {if(w->mode1==1)en09_brain(w,&plp);if(w->mode0==1)en09_move_state(w,&plp);}
}

static void en09_damage(RECVX_ENEMY_WORK* w) { en09_damage_state(w); } /* :2466 */
static void en09_nage(RECVX_ENEMY_WORK* w)   { en09_nage_state(w); } /* :2472 */
static void en09_die(RECVX_ENEMY_WORK* w)    { en09_die_state(w); } /* :2478 */

/* en09.c:2112 bhEne09_PlayerControl */
static void en09_player_control(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_EN09_EXT* e = en09_ext(w);
    RECVX_ENEMY_PLAYER pl;

    if (!e || !(e->state & RECVX_EN09_ST_GRAB)) return;
    if (!sv || !sv->player_get || !sv->player_get(&pl)) {
        en09_gap(w, "plp (bhEne09_PlayerControl)");
        return;
    }
    if (pl.mode0 != 4u && pl.mode0 != 6u) return;

    en09_player_grab(w,&pl);
}

/* en09.c:2099 bhEne09_MainLoop */
static void en09_main_loop(RECVX_ENEMY_WORK* w)
{
    RECVX_EN09_EXT* e;

    switch (w->mode0) {                                    /* :2101 Mode0[6] */
    case 0: en09_init(w);   break;
    case 1: en09_move(w);   break;
    case 2: en09_nage(w);   break;
    case 3: en09_damage(w); break;
    case 4: en09_die(w);    break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);        /* subpl.c:143 */
        else                       en09_gap(w, "bhEne_Event");
        break;
    default:
        en09_gap(w, "bhEne09_Mode0: mode0 ausserhalb der Tabelle");
        return;
    }

    en09_player_control(w);                                /* :2103 */

    e = en09_ext(w);
    if (e && !(e->state & RECVX_EN09_ST_NO_SETMTN)) (void)en09_set_mtn(w);  /* :2105 */
}

/* ---------------------------------------------------------------------------
   en09.c:2037 bhEne09 — der Rahmen eines Passes
   --------------------------------------------------------------------------- */
void recvx_en09(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_EN09_EXT* e;
    int i;

    if (!w) return;
    sv = w->sv;

    en09_damage_check(w);                       /* :2042 */
    en09_main_loop(w);                                     /* :2043 */

    if (w->flg & 4u) {                                     /* :2045 */
        for (i = 0; i < (int)RECVX_ENEMY_DAM; ++i) w->dam[i] = 0;
        w->flg &= ~4u;
    }

    e = en09_ext(w);
    if (e && e->ct_14 > 0) e->ct_14--;                     /* :2055 */

    en09_coll_check(w);                                    /* :2060 */
    en09_calc_enemy(w);                                    /* :2061 */
    en09_player_link(w);                     /* :2062 */

    e = en09_ext(w);
    if (e && (e->state & RECVX_EN09_ST_ARM_OUT)) {         /* :2064 */
        RECVX_ENEMY_PLAYER pl;
        if (sv && sv->player_get && sv->player_get(&pl)) {
            if (!(pl.flg & 4u) && !(pl.stflg & 0x80000000u)){
                if(en09_arm_contact(w,&pl.pos,pl.car)&&sv->player_set)sv->player_set(&pl);
            }
        } else en09_gap(w, "plp (bhEne09 Armkollision)");

        /* :2071-2084 — dieselbe Pruefung gegen alle uebrigen Gegner der
           Originalliste; id 0 (Fass) und id 0x3D sind ausgenommen. */
        if (sv && sv->enemy_count && sv->enemy_at) {
            const uint32_t n = sv->enemy_count();
            for (i = 0; i < (int)n; ++i) {
                RECVX_ENEMY_WORK* ep = sv->enemy_at((uint32_t)i);
                if (!ep || ep == w) continue;
                if (ep->id == 0u || ep->id == 0x3Du) continue;
                if ((ep->flg & 2u) || (ep->flg2 & 1u) || !(ep->flg & 1u)) continue;
                RECVX_ENEMY_VEC pos=en09_pos(ep);
                if(en09_arm_contact(w,&pos,ep->car)){ep->px=pos.x;ep->pz=pos.z;}

            }
        } else en09_gap(w, "sys->ene/ewk_n");
    }

    e = en09_ext(w);
    if (e && e->ct_1c > 0) e->ct_1c--;                     /* :2087 */
    if (e && e->ct_20 > 0) e->ct_20--;                     /* :2092 */
}
