/* EN02 Gulp Worm — eigenstaendige native Zustandsmaschine auf dem einzigen
   WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en02.c:714 bhEne02, :747 Init, :851 Brain/:857 BR00,
   :950 Move, :968 MV00, :1056 MV02, :1353 MV03, :1595 MV04, :1777 MV05,
   :1812 Damage, :1868 DG01, :1902 Die, :1908 DD00, :2202 DD01,
   :2252-2660 Sandeffekte, :2661 PlayerControl, :2944 DamageInit,
   :2997 SetSandParticle, :3043 CheckWall, :3074 CameraControl, :3102 WarpCheck,
   :3137 FallingRock, :3171 CallSE, :3291 HitMark; en02sub.c:40-271 bhEne02sub
   (Maul/Ring/Felsen, Identitaet 31). Datentabellen in en02_tables.c.
   Zufallsreihenfolge je Ausdruck wie im Original (links nach rechts); die
   beiden Zufallsargumente von bhEne_SetFireEffect in der Reihenfolge, die
   EN23/EN05 gegen die Disassembly belegt haben (size vor len).
   Ungebundene Dienste melden den Originalnamen und erfinden keinen Wert. */
#include "en02.h"
#include "en02_tables.h"
#include "en02_trans_tables.h"
#include "en03_lib.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <stddef.h>
#include <string.h>

static const RECVX_EN02_SERVICES* en02_sv;
void recvx_en02_bind(const RECVX_EN02_SERVICES* services) { en02_sv = services; }

static void gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
#define SV(w, f, name)    do { if (!en02_sv || !en02_sv->f) { gap(w, name); return; } } while (0)
#define SVI(w, f, name)   do { if (!en02_sv || !en02_sv->f) { gap(w, name); return 0; } } while (0)
#define BASE(w, f, name)  do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return; } } while (0)
#define BASEI(w, f, name) do { if (!(w)->sv || !(w)->sv->f) { gap(w, name); return 0; } } while (0)

static RECVX_EN02_STATE* st(RECVX_ENEMY_WORK* w) { return (RECVX_EN02_STATE*)w->exp0; }

/* (-rand() / -2147483648.0f): rand()/2^31 als float, gemeinsamer EE-Strom. */
static int rnd(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) { gap(w, "rand (en02)"); return 0; }
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) { gap(w, "plp (en02)"); return 0; }
    return 1;
}
static void put_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    if (w->sv && w->sv->player_set) w->sv->player_set(p);
    else gap(w, "plp schreiben (en02)");
}
static int frames(RECVX_ENEMY_WORK* w, uint32_t mtn, uint32_t* out)
{
    BASEI(w, motion_frames, "epw->mnwP[mtn_no].frm_num (en02)");
    *out = w->sv->motion_frames(w, mtn);
    if (!*out) { gap(w, "epw->mnwP[mtn_no] fehlt (en02)"); return 0; }
    return 1;
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t j, RECVX_ENEMY_VEC* out)
{
    if (!recvx_t3_joint_pos(w, j, out)) { gap(w, "mlwP->owP[n].mtx (en02)"); return 0; }
    return 1;
}
/* njUnitMatrix; njRotateY(ay); njCalcVector. */
static void rot_y(int ay, const RECVX_ENEMY_VEC* in, RECVX_ENEMY_VEC* out)
{
    NJS_MATRIX m; NJS_VECTOR v = { in->x, in->y, in->z }, o;
    njUnitMatrix(&m); njRotateY(&m, (Angle)ay); njCalcVector(&m, &v, &o);
    out->x = o.x; out->y = o.y; out->z = o.z;
}
static int vib(RECVX_ENEMY_WORK* w, int motor, int power)
{
    BASEI(w, vibration, "StartVibrationEx (en02)");
    w->sv->vibration(motor, power);
    return 1;
}
/* Fusspunkt des Spielers auf der Geraden durch `lp` in Richtung (-sin ay, 0, -cos ay). */
static void line_foot(const RECVX_ENEMY_VEC* lp, int ay, const RECVX_ENEMY_VEC* ppos, RECVX_ENEMY_VEC* foot)
{
    RECVX_EN03_LINE ln;
    ln.p = *lp;
    ln.v.x = -njSin((Angle)ay); ln.v.y = 0.0f; ln.v.z = -njCos((Angle)ay);
    recvx_en03_distance_p2l(ppos, &ln, foot);
}
static float scalor(float x, float y, float z) { NJS_VECTOR v = { x, y, z }; return njScalor(&v); }

/* plp->dvx.. = pos - plp->px.. und bhEne_DGDirCheck(plp) (zonzon1.c:515 liest
   plp->ay und plp->dvx/dvz). Die Trefferfelder gehoeren dem Spielerbesitzer.
   -1 = Felder ohne Besitzer (gemeldet). */
static int player_dv(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p,
                     const RECVX_ENEMY_VEC* pos, RECVX_ENEMY_VEC* dv)
{
    int djnt; RECVX_ENEMY_VEC odv, dp;
    if (!recvx_t3_player_hit_get(&djnt, &odv, &dp)) { gap(w, "plp->djnt_no/dpx (en02)"); return -1; }
    dv->x = pos->x - p->pos.x; dv->y = pos->y - p->pos.y; dv->z = pos->z - p->pos.z;
    if (!recvx_t3_player_hit_set(djnt, dv, &dp)) { gap(w, "plp->dvx (en02)"); return -1; }
    return recvx_t3_dir_check(p->ay, dv);
}
static int player_djnt(RECVX_ENEMY_WORK* w, int* djnt)
{
    RECVX_ENEMY_VEC dv, dp;
    if (!recvx_t3_player_hit_get(djnt, &dv, &dp)) { gap(w, "plp->djnt_no (en02)"); return 0; }
    return 1;
}
/* Niederschlag (mode0 4, mode2 0/1) bzw. Tod (mode0 6, mode2 2/3) mit ayp aus dv
   (en02.c:1243-1283 u.a.). `back` = bhEne_DGDirCheck(plp). */
static void knock_dir(RECVX_ENEMY_PLAYER* p, int back, int death, const RECVX_ENEMY_VEC* dv)
{
    p->flg |= 0x10004u;
    p->stflg |= death ? 0x50000u : 0x10000u;
    p->mode0 = death ? 6 : 4; p->mode1 = 0; p->mode3 = 0;
    if (back) { p->ayp = recvx_t3_arc_tan2(-dv->x, -dv->z); p->mode2 = death ? 3 : 1; }
    else      { p->ayp = recvx_t3_arc_tan2(dv->x, dv->z);   p->mode2 = death ? 2 : 0; }
}
static void dir_vec(const RECVX_ENEMY_WORK* w, float len, int ay, RECVX_ENEMY_VEC* out)
{
    out->x = w->px - len * njSin((Angle)ay);
    out->y = w->py;
    out->z = w->pz - len * njCos((Angle)ay);
}

/* --- Sandeffekte (en02.c:2252-2660, :2997) ------------------------------ */

/* en02.c:2500 bhEne02_SetSandSpr. Ein Zufallswert erst NACH erfolgreichem
   bhSetEffectTb (eno != -1), wie im Original. */
static void sand_spr(RECVX_ENEMY_WORK* w, int type0, const RECVX_ENEMY_VEC* pos,
                     int type, int flip, float size, int ang)
{
    RECVX_EN02_EFF ef; RECVX_EN02_EFFSET set; int eno; float r;
    SV(w, effect_tb, "bhSetEffectTb (en02 SandSpr)");
    SV(w, effect_set, "eff[eno].xn/yn/zn (en02 SandSpr)");
    memset(&ef, 0, sizeof ef);
    ef.id = 253; ef.type = (uint32_t)type; ef.flg = 1;             /* :2504-2508 */
    ef.pos = *pos;
    ef.sx = ef.sy = ef.sz = size;                                   /* :2514-2516 */
    ef.mdlver = (uint32_t)type0;                                    /* :2518 */
    if (flip & 0x1) ef.sx *= -1.0f;                                 /* :2520-2528 */
    if (flip & 0x2) ef.sz *= -1.0f;
    eno = en02_sv->effect_tb(RECVX_EN02_EF_ID | RECVX_EN02_EF_TYPE | RECVX_EN02_EF_FLG |
                             RECVX_EN02_EF_POS | RECVX_EN02_EF_SCALE | RECVX_EN02_EF_MDLVER, &ef);
    if (eno == -1) return;                                          /* :2530 */
    if (!rnd(w, &r)) return;
    memset(&set, 0, sizeof set);
    set.xn = -njSin((Angle)ang) * 0.5f;                             /* :2535 */
    set.zn = -njCos((Angle)ang) * 0.5f;                             /* :2536 */
    set.yn = 0.3f + (0.6f * r);                                     /* :2537 */
    en02_sv->effect_set(eno, RECVX_EN02_EFS_VEC, &set);
}
/* en02.c:2997 bhEne02_SetSandParticle. sys->ef.sx/sy/sz/mdlver bleiben vom
   vorigen Aufruf stehen (das Original schreibt sie hier nicht). */
static void sand_particle(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int type)
{
    static const int   col[6] = { 0, 1, 2, 0, 1, 2 };
    static const float vy[6]  = { 2.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.5f };
    RECVX_EN02_EFF ef; RECVX_EN02_EFFSET set; int eno;
    SV(w, effect_tb, "bhSetEffectTb (en02 SandParticle)");
    SV(w, effect_set, "eff[eno] Nachfelder (en02 SandParticle)");
    if (type < 0 || type > 5) { gap(w, "bhEne02_SetSandParticle: type ausserhalb col[6]"); return; }
    memset(&ef, 0, sizeof ef);
    ef.id = 254; ef.flg = 1; ef.type = (uint32_t)col[type];          /* :3010-3014 */
    ef.pos = *pos;
    eno = en02_sv->effect_tb(RECVX_EN02_EF_ID | RECVX_EN02_EF_FLG | RECVX_EN02_EF_TYPE | RECVX_EN02_EF_POS, &ef);
    if (eno < 0) return;                                            /* :3022 */
    set.xn = 0.0f; set.yn = vy[type]; set.zn = 0.0f;                /* :3029-3031 */
    set.sx = set.sy = set.sz = 8.0f;                                /* :3033-3035 */
    set.sxb = 1.0f; set.syb = 0.6f * vy[type]; set.szb = 1.0f;      /* :3037-3039 */
    en02_sv->effect_set(eno, RECVX_EN02_EFS_VEC | RECVX_EN02_EFS_SCALE | RECVX_EN02_EFS_SCALEB, &set);
}
/* en02.c:2264 bhEne02_SetSandEffectMain. */
static void sand_main(RECVX_ENEMY_WORK* w, int type0, const RECVX_ENEMY_VEC* pos, int type)
{
    RECVX_ENEMY_VEC wp; int i, ang; float r, a, s;
    switch (type) {
    case 0: case 1:                                                 /* :2273 */
        for (i = 0; i < 6; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + (8.0f * a);
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, type, 0, 1.0f, ang);
        }
        sand_particle(w, &wp, 0); sand_particle(w, &wp, 1);          /* :2288-2289 */
        break;
    case 2:                                                         /* :2291 */
        for (i = 0; i < 3; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 6.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + (6.0f * a);
            wp.z = pos->z - (r * njCos((Angle)ang));
            if (!rnd(w, &s)) return;
            sand_spr(w, type0, &wp, 2, i & 3, 0.7f + (0.3f * s), ang);
        }
        for (i = 0; i < 2; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = 6.0f + (pos->y + (8.0f * a));
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
            sand_particle(w, &wp, 0); sand_particle(w, &wp, 1); sand_particle(w, &wp, 2);
        }
        break;
    case 3:                                                         /* :2325 */
        for (i = 0; i < 3; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 6.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + a;
            wp.z = pos->z - (r * njCos((Angle)ang));
            if (!rnd(w, &s)) return;
            sand_spr(w, type0, &wp, 2, i & 3, 0.7f + (0.3f * s), ang);
        }
        for (i = 0; i < 2; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = 6.0f + (pos->y + (8.0f * a));
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
            sand_particle(w, &wp, 3); sand_particle(w, &wp, 4); sand_particle(w, &wp, 5);
        }
        break;
    case 4:                                                         /* :2359 */
        for (i = 0; i < 6; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = (pos->y + (16.0f * a)) - 8.0f;
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
        }
        break;
    case 5:                                                         /* :2374 */
        for (i = 0; i < 8; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = (pos->y + (16.0f * a)) - 8.0f;
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 1, 0, 1.0f, ang);
        }
        break;
    case 6:                                                         /* :2389 */
        for (i = 0; i < 3; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 6.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + a;
            wp.z = pos->z - (r * njCos((Angle)ang));
            if (!rnd(w, &s)) return;
            sand_spr(w, type0, &wp, 2, i & 3, 0.7f + (0.3f * s), ang);
        }
        for (i = 0; i < 2; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 8.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = 6.0f + (pos->y + (8.0f * a));
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
        }
        break;
    case 7:                                                         /* :2419 */
        for (i = 0; i < 3; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 2.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + (5.0f * a);
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 1, 0, 1.0f, ang);
        }
        for (i = 0; i < 4; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 3.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + (5.0f * a);
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
        }
        break;
    case 8:                                                         /* :2446 */
        /* ORIGINAL-FIX (QUALITY.md): en02.c:2452/2462-2463 lesen `ang`, ohne es in
           diesem Fall zu setzen (Originalkommentar). Disassembly 0x193AE8-0x193D5C:
           ang = $s3 beim Funktionseintritt; weder die Kette bis bhControlEnemy noch
           bhMainSequence/bhInitEvent schreiben $s3, der Wert ist also Fremdinhalt
           und dreht xn/zn der Sprites zufaellig. Regel 4: alle anderen Faelle
           berechnen ang aus rand() -> 0, d.h. xn 0, zn -0.5. Die sieben SetSandSpr
           laufen wieder in Originalreihenfolge (vorher fehlten mit ihnen auch die
           rand()-Aufrufe fuer yn und die eff[]-Plaetze). */
        ang = 0;
        for (i = 0; i < 3; i++) {
            if (!rnd(w, &a)) return; wp.x = (pos->x + (12.0f * a)) - 6.0f;
            if (!rnd(w, &a)) return; wp.y = pos->y + a;
            if (!rnd(w, &a)) return; wp.z = (pos->z + (12.0f * a)) - 6.0f;
            if (!rnd(w, &s)) return;
            sand_spr(w, type0, &wp, 2, i & 1, 0.7f + (0.3f * s), ang);  /* :2452 */
        }
        for (i = 0; i < 2; i++) {
            if (!rnd(w, &a)) return; wp.x = (pos->x + (16.0f * a)) - 8.0f;
            if (!rnd(w, &a)) return; wp.y = 2.0f + (pos->y + (8.0f * a));
            if (!rnd(w, &a)) return; wp.z = (pos->z + (16.0f * a)) - 8.0f;
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);               /* :2462 */
            sand_spr(w, type0, &wp, 0, 1, 1.0f, ang);               /* :2463 */
            sand_particle(w, &wp, 3); sand_particle(w, &wp, 4); sand_particle(w, &wp, 5);
        }
        break;
    case 9:                                                         /* :2471 */
        for (i = 0; i < 4; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r *= 7.0f;
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = pos->y + a;
            wp.z = pos->z - (r * njCos((Angle)ang));
            if (!rnd(w, &s)) return;
            sand_spr(w, type0, &wp, 2, i & 3, 0.7f + (0.3f * s), ang);
        }
        for (i = 0; i < 2; i++) {
            if (!rnd(w, &a) || !rnd(w, &r)) return;
            ang = (int)(65536.0f * a); r = 10.0f + (4.0f * r);
            wp.x = pos->x - (r * njSin((Angle)ang));
            if (!rnd(w, &a)) return; wp.y = 6.0f + (pos->y + (8.0f * a));
            wp.z = pos->z - (r * njCos((Angle)ang));
            sand_spr(w, type0, &wp, 0, 0, 1.0f, ang);
            sand_particle(w, &wp, 3); sand_particle(w, &wp, 4); sand_particle(w, &wp, 5);
        }
        break;
    default: break;
    }
}
void recvx_en02_set_sand_effect(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* pos, int type)
{
    if (!w) return;
    sand_main(w, (w->type == 0) ? 0 : 1, pos, type);               /* :2254 */
}
void recvx_en02_set_sand_effect_ev(RECVX_ENEMY_WORK* ene, int type1, int type2)
{
    RECVX_ENEMY_VEC p;
    if (!ene) return;
    p.x = ene->px; p.y = ene->py; p.z = ene->pz;                    /* :2260 */
    sand_main(ene, type1, &p, type2);
}
/* en02.c:2542 bhEne02_SandEffect: Zeilen der laufenden Gegnermotion. */
static void sand_effect_mtn(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN02_SANDTBL* tb; const RECVX_EN02_WE* we; uint32_t fno, i;
    RECVX_ENEMY_VEC ofp, r;
    fno = (uint32_t)(w->frm_no / 65536);
    for (tb = recvx_en02_sand_tbl; tb->mtn_no != -1; tb++) {
        if (w->mtn_no != (uint32_t)tb->mtn_no) continue;
        for (i = 0, we = tb->we; i < tb->num; i++, we++) {
            if (we->frm_no != fno) continue;
            ofp.x = we->ofx; ofp.y = we->ofy; ofp.z = we->ofz;
            rot_y(w->ay, &ofp, &r);
            r.x += w->px; r.y += w->py; r.z += w->pz;
            recvx_en02_set_sand_effect(w, &r, (int)we->size);
        }
    }
}
/* en02.c:2588 bhEne02_SandEffectP: Zeilen der laufenden SPIELERmotion. */
static void sand_effect_player(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    const RECVX_EN02_SANDTBL* tb; const RECVX_EN02_WE* we; uint32_t fno, i; int j;
    RECVX_ENEMY_VEC ofp, r, pos; float a;
    const int type = (w->type != 0) ? 1 : 0;
    fno = (uint32_t)(p->frm_no / 65536);
    for (tb = recvx_en02_sand_tbl; tb->mtn_no != -1; tb++) {
        if (p->mtn_no != (uint32_t)tb->mtn_no) continue;
        for (i = 0, we = tb->we; i < tb->num; i++, we++) {
            if (we->frm_no != fno) continue;
            ofp.x = we->ofx; ofp.y = we->ofy; ofp.z = we->ofz;
            rot_y(p->ay, &ofp, &r);
            r.x += p->pos.x; r.y += p->pos.y; r.z += p->pos.z;
            if (we->size == 0) {                                    /* :2628 */
                for (j = 0; j < 2; j++) {
                    if (!rnd(w, &a)) return; pos.x = (r.x + (4.0f * a)) - 2.0f;
                    if (!rnd(w, &a)) return; pos.y = (r.y + (4.0f * a)) - 2.0f;
                    if (!rnd(w, &a)) return; pos.z = (r.z + (4.0f * a)) - 2.0f;
                    sand_spr(w, type, &pos, 0, 0, 1.0f, 0);
                }
                for (j = 0; j < 2; j++) {
                    if (!rnd(w, &a)) return; pos.x = (r.x + (2.0f * a)) - 1.0f;
                    if (!rnd(w, &a)) return; pos.y = (r.y + (2.0f * a)) - 1.0f;
                    if (!rnd(w, &a)) return; pos.z = (r.z + (2.0f * a)) - 1.0f;
                    sand_spr(w, type, &pos, 1, 0, 1.0f, 0);
                }
            }
        }
    }
}

/* --- Init (en02.c:747) ---------------------------------------------------- */
static void init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s; RECVX_ENEMY_PLAYER p; int gm, i;
    w->flg |= 0x100018u;                                            /* :751 */
    w->flg &= ~0x400000u;                                           /* :753 */
    w->flg &= ~0x6u;
    w->flg2 |= 0x1u;                                                /* :756 */
    w->ar = 15.0f; w->ah = 10.0f;                                   /* :758-759 */
    w->car = 10.0f; w->cah = 10.0f;                                 /* :761-762 */
    SV(w, game_mode, "bhEne02_Init: sys->gm_mode");
    if (!en02_sv->game_mode(&gm)) { gap(w, "bhEne02_Init: sys->gm_mode"); return; }
    w->hp = (gm != 2) ? 300 : 200;                                  /* :764 */
    w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;         /* :766-769 */
    w->mtn_md |= 0x20u;                                             /* :771 */
    if (!w->exp0) {                                                 /* :773 */
        RECVX_ENEMY_TEMPLATE t; RECVX_ENEMY_ATR atr; RECVX_ENEMY_WORK* c;
        BASE(w, calloc_work, "bhEne_CallocWork (en02)");
        SV(w, create_child, "bhEne02_Init: eneset.c:170 Kindwork");
        SV(w, morph_sources, "bhEne02_Init: obj_a/obj_b (mdl[5]/mdl[6])");
        BASE(w, mobile_wall_alloc, "bhEne02_Init: sys->mwalp[sys->mwal_n]");
        BASE(w, mobile_wall_set, "bhEne02_Init: sys->mwalp Felder");
        if (!recvx_enemy_ext() || !recvx_enemy_ext()->set_call_func) { gap(w, "bhEne_SetCallFunc(bhEne02sub, 31)"); return; }
        w->exp0 = w->sv->calloc_work((uint32_t)sizeof(RECVX_EN02_STATE)); /* :775 bhEne_CallocWork(144, 8) */
        if (!w->exp0) { gap(w, "bhEne02_Init: EXP-Budget erschoepft"); return; }
        s = st(w);
        memset(&t, 0, sizeof t);
        t.flg = 0x8001u; t.id = 31; t.type = 0;                     /* en02.c:223 child = { 0x8001, 31, 0 } */
        /* Maul: type 0, mlwP = &mdl[5], obj_a = mdl[5].objP, obj_b = mdl[6].objP (:777-782) */
        c = en02_sv->create_child(w, &t, 5u);
        if (!c) { gap(w, "bhEne02_Init: ene[] voll (Maul)"); return; }
        c->type = 0; c->lkwkp = w; s->mouth = c;
        en02_sv->morph_sources(c, 0u, 1u);
        /* Ring: type 1, mlwP = &mdl[4] (:784-789) */
        c = en02_sv->create_child(w, &t, 4u);
        if (!c) { gap(w, "bhEne02_Init: ene[] voll (Ring)"); return; }
        c->type = 1; c->lkwkp = w; s->ring = c;
        /* 16 Felsen: type 2, mlwP = &mdl[1] (:791-799) */
        for (i = 0; i < (int)RECVX_EN02_ROCKS; i++) {
            c = en02_sv->create_child(w, &t, 1u);
            if (!c) { gap(w, "bhEne02_Init: ene[] voll (Felsen)"); return; }
            c->type = 2; c->lkwkp = w; s->rocks[i] = c;
        }
        recvx_enemy_ext()->set_call_func(31u, recvx_en02sub);       /* :801 */
        /* &sys->mwalp[sys->mwal_n++]: flg 0, type 3, w 10, h 0, d 0, attr 0 (:803-815) */
        s->wall_index = w->sv->mobile_wall_alloc();
        if (s->wall_index < 0) { gap(w, "bhEne02_Init: sys->mwalp erschoepft"); return; }
        memset(&atr, 0, sizeof atr);
        atr.flg = 0; atr.type = 3; atr.w = 10.0f; atr.h = 0.0f; atr.d = 0.0f; atr.attr = 0;
        w->sv->mobile_wall_set(s->wall_index, &atr);
    }
    s = st(w);
    s->attack_ct = 1;                                               /* :821 EXP0_I(112) */
    s->warp_floor = -1;                                             /* :822 EXP0_I(116) = 0 (NULL) */
    s->car_target = 10.0f;                                          /* :824 EXP0_F(120) */
    w->clp_jno[0] = 11; w->clp_jno[1] = 9; w->clp_jno[2] = 7; w->clp_jno[3] = 5; /* :826-833 */
    w->clp_jno[4] = 4;  w->clp_jno[5] = 3; w->clp_jno[6] = 2; w->clp_jno[7] = 1;
    w->mdflg &= ~0x20u;                                             /* :835 */
    w->lok_jno = 11;                                                /* :837 */
    w->flg &= ~0x8000u;                                             /* :839-840 */
    w->flg &= ~0x80000u;
    w->cpcl = recvx_en02_cap_col;                                   /* :842 */
    /* plp->mdflg &= ~0x1; plp->stflg &= ~0x8; plp->stflg &= ~0x1000000 (:844-847) */
    if (en02_sv->player_mdflg_clear) { if (!en02_sv->player_mdflg_clear(0x1u)) gap(w, "plp->mdflg &= ~0x1 (en02.c:844)"); }
    else gap(w, "plp->mdflg &= ~0x1 (en02.c:844)");
    if (!player(w, &p)) return;
    p.stflg &= ~0x8u;
    p.stflg &= ~0x1000000u;
    put_player(w, &p);
}

/* --- Brain (en02.c:851/857) ----------------------------------------------- */
static void warp_check(RECVX_ENEMY_WORK* w);
static void brain(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC vec, pos; int i; float r;
    if (!player(w, &p)) return;
    if (!((w->flg & 0x100000u) && p.mode0 != 6)) return;            /* :863 */
    warp_check(w);                                                  /* :865 */
    BASE(w, wall_type, "bhCheckWallType (en02 BR00)");
    if (s->attack_ct == 0) {                                        /* :867 */
        pos.x = w->px; pos.y = w->py; pos.z = w->pz;
        vec.x = -10.0f * njSin((Angle)w->ay); vec.y = 0.0f; vec.z = -10.0f * njCos((Angle)w->ay);
        for (i = 0; i < 4; i++) {                                   /* :877 */
            if (w->sv->wall_type(&pos, 0u, 10.0f, 5.0f)) goto label;
            pos.x += vec.x; pos.y += vec.y; pos.z += vec.z;
        }
        pos.x = (w->px - (20.0f * njSin((Angle)w->ay))) - p.pos.x;  /* :887-889 */
        pos.z = (w->pz - (20.0f * njCos((Angle)w->ay))) - p.pos.z;
        pos.y = 0.0f;
        if (scalor(pos.x, pos.y, pos.z) < 20.0f) {                  /* :891 */
            w->mode1 = 0; w->mode2 = 2; w->mode3 = 0;
            if (!rnd(w, &r)) return;
            if (p.hp > 40) s->attack_ct = (int)(2.0f * r) + 1;      /* :899 */
            else           s->attack_ct = (int)(3.0f * r);          /* :903 */
            return;
        }
    }
label:                                                              /* :910 */
    pos.x = w->px; pos.y = w->py; pos.z = w->pz;
    vec.x = -10.0f * njSin((Angle)w->ay); vec.y = 0.0f; vec.z = -10.0f * njCos((Angle)w->ay);
    for (i = 0; i < 4; i++) {
        if (w->sv->wall_type(&pos, 0u, 10.0f, 5.0f)) return;
        pos.x += vec.x; pos.y += vec.y; pos.z += vec.z;
    }
    pos.x = (w->px - (30.0f * njSin((Angle)w->ay))) - p.pos.x;      /* :929-931 */
    pos.z = (w->pz - (30.0f * njCos((Angle)w->ay))) - p.pos.z;
    pos.y = 0.0f;
    if (scalor(pos.x, pos.y, pos.z) < 23.0f) {                      /* :933 */
        w->mode1 = 0; w->mode2 = 3; w->mode3 = 0;
        if (s->attack_ct != 0) s->attack_ct--;                      /* :939-942 */
    }
}

/* --- Move / MV00-05 (en02.c:950-1805) ------------------------------------- */
static void mv00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; float px, pz, r;
    switch (w->mode3) {
    case 0:
        SV(w, room_flags, "sys->rm_flg (en02 MV00)");
        en02_sv->room_flags(0u, 0x1u);                              /* :975 sys->rm_flg &= ~1 */
        w->flg &= ~0x60u;                                           /* :977 */
        w->flg |= 0x180000u; w->flg |= 0x8000u;                     /* :979-980 */
        w->mtn_no = 1; w->frm_no = 0; w->mtn_add = 0;               /* :982-985 */
        if (!rnd(w, &r)) return; w->ct0 = (int)(30.0f * r) + 40;    /* :987 */
        w->ct1 = 0;
        if (!rnd(w, &r)) return; w->ct2 = (int)(20.0f * r) + 60;    /* :989 */
        w->axp = 5461;                                              /* :991 */
        s->shake = 0.3f;                                            /* :993 */
        w->mode3++;
        /* kein break (:996) */
    case 1:
        if (!player(w, &p)) return;
        if (s->attack_ct == 0) {                                    /* :997 */
            px = w->px - (40.0f * njSin((Angle)w->ay)); pz = w->pz - (40.0f * njCos((Angle)w->ay));
        } else {
            px = w->px - (5.0f * njSin((Angle)w->ay));  pz = w->pz - (5.0f * njCos((Angle)w->ay));
        }
        w->px += (p.pos.x - px) / 16.0f;                            /* :1008-1009 */
        w->pz += (p.pos.z - pz) / 16.0f;
        w->ayp = recvx_t3_arc_tan2(px - p.pos.x, pz - p.pos.z);     /* :1011 */
        w->ay += ((int16_t)(w->axp + (w->ayp - w->ay))) / 16;       /* :1013 */
        if (w->ct0-- == 0) w->mode1 = 1;                            /* :1015 */
        if (w->ct1-- == 0) { w->axp = -w->axp; w->ct1 = 8; }        /* :1020 */
        if (w->ct2-- == 0) {                                        /* :1027 */
            w->px -= 40.0f * njSin((Angle)w->ay); w->pz -= 40.0f * njCos((Angle)w->ay);
            w->ay = w->ayp + 32768;
            w->px += 40.0f * njSin((Angle)w->ay); w->pz += 40.0f * njCos((Angle)w->ay);
            if (!rnd(w, &r)) return; w->ct2 = (int)(20.0f * r) + 20;
        }
        {   /* :1040 ChechPlayEnemySe(sys->enow, 70400) */
            RECVX_ENEMY_VEC wp = { w->px, w->py, w->pz };
            BASE(w, enemy_se_playing, "ChechPlayEnemySe (en02 MV00)");
            BASE(w, enemy_now, "sys->enow (en02 MV00)");
            if (w->sv->enemy_se_playing(w->sv->enemy_now(), 70400) == 0) recvx_t3_call_se_ex(w, &wp, 70400, 60);
            else { SV(w, se_pan, "bhEne_SetSEPan (en02 MV00)"); en02_sv->se_pan(w->sv->enemy_now(), &wp, 70400); }
        }
        break;
    default: break;
    }
}
/* en02.c:1056 bhEne02_MV02 — Auftauchen, Schlucken, Zurueckfallen. */
static void mv02(RECVX_ENEMY_WORK* w)
{
    static const RECVX_ENEMY_VEC trg_pos[2] = { { 0.0f, 0.0f, -10.18284797668457f }, { 0.0f, 0.0f, -10.064862251281738f } };
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC pos, jp, dv, foot, wp;
    int i, back, djnt; uint32_t fno;
    switch (w->mode3) {
    case 0:
        SV(w, room_flags, "sys->rm_flg (en02 MV02)");
        en02_sv->room_flags(0x1u, 0u);                              /* :1070 */
        w->flg |= 0x40u; w->flg |= 0x80000u;                        /* :1072-1073 */
        w->flg &= ~0x100000u; w->flg &= ~0x8000u;                   /* :1075-1076 */
        w->flg |= 0x20u;                                            /* :1078 */
        w->mtn_no = 14; w->frm_no = 0; w->mtn_add = 65536;          /* :1080-1083 */
        w->mode3++;
        if (!vib(w, 1, 11)) return;                                 /* :1087 */
        break;
    case 1:
        if (w->frm_no == 196608) { s->mouth->mode2 = 1; s->mouth->mode3 = 0; } /* :1090 */
        if (!player(w, &p)) return;
        if (w->frm_no == 0) {                                       /* :1096 */
            w->mtn_no = 16; w->mode3++;
            if (!joint(w, 11u, &jp)) return;
            if ((p.hp < 60) && !(p.flg & 0x4u) && recvx_t3_attack_hit_check(&jp, 17.0f)) { /* :1102 */
                p.flg |= 0x10004u; p.stflg |= 0x50000u;             /* :1104-1105 */
                p.flg &= ~0x8u;                                     /* :1107 */
                p.hp = -1;                                          /* :1109 */
                p.mode0 = 6; p.mode1 = 0; p.mode3 = 0;              /* :1111-1113 */
                wp.x = w->px; wp.y = w->py; wp.z = w->pz;
                back = player_dv(w, &p, &wp, &dv);                  /* :1115-1119 */
                if (back < 0) return;
                if (back) { p.mode2 = 1; p.ayp = w->ay + 32768; }   /* :1121-1124 */
                else      { p.mode2 = 0; p.ayp = w->ay; }
                p.stflg |= 0x8u;                                    /* :1133 */
                w->mtn_no = 17; w->mode3 = 3;                       /* :1135-1137 */
                rot_y(w->ay, &trg_pos[p.mode2], &pos);              /* :1139-1143 */
                s->swallow_x = w->px + pos.x;                       /* :1145-1146 plp->xn/zn */
                s->swallow_z = w->pz + pos.z;
                w->ct0 = 8;                                         /* :1148 */
                { const RECVX_T3_LINK lk = { NULL, 1 }; recvx_t3_set_blood_effect_burst(lk, 3, 1, NULL, 0); } /* :1150 */
                put_player(w, &p);
                if (!vib(w, 1, 11)) return;                         /* :1152 */
                return;
            }
            put_player(w, &p);
        }
        fno = (uint32_t)(w->frm_no / 65536);
        s->shake = (fno < 5) ? 4.2f : 2.2f;                         /* :1157-1163 */
        break;
    case 2:
        if (w->frm_no == 0) {                                       /* :1175 */
            w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
            w->mtn_no = 1; w->mtn_add = 0;
            w->flg |= 0x100000u;
        }
        fno = (uint32_t)(w->frm_no / 65536);
        if (fno == 15) { s->mouth->mode2 = 2; s->mouth->mode3 = 0; } /* :1187 */
        if (!player(w, &p)) return;
        if ((fno < 44) && !(p.flg & 0x4u)) {                        /* :1193 */
            for (i = 0; i < 9; i++) {                               /* owP[3..11] */
                float mtx[16];
                BASE(w, joint_matrix, "mlwP->owP[n].mtx (en02 MV02)");
                if (!w->sv->joint_matrix(w, (uint32_t)(3 + i), mtx)) { gap(w, "mlwP->owP[3+i].mtx (en02 MV02)"); return; }
                jp.x = mtx[12]; jp.y = mtx[13]; jp.z = mtx[14];
                if (((mtx[13] - 6.0f) >= p.pos.y) && recvx_t3_attack_hit_check(&jp, 12.0f)) { /* :1199 */
                    float dist;
                    p.flg |= 0x10004u; p.stflg |= 0x10000u;         /* :1201-1202 */
                    if (i > 7) {                                    /* :1204 */
                        p.hp -= 30;
                        if (!player_djnt(w, &djnt)) return;
                        { const RECVX_T3_LINK lk = { NULL, 1 }; recvx_t3_set_blood_effect_burst(lk, 3, djnt, NULL, 0); }
                    } else {
                        p.hp -= 10;
                        if (p.hp < 0) p.hp = 0;
                    }
                    wp.x = w->px; wp.y = w->py; wp.z = w->pz;
                    line_foot(&wp, w->ay, &p.pos, &foot);           /* :1218-1226 */
                    dist = recvx_en03_distance_p2p(&wp, &foot);     /* :1228 */
                    pos = foot;
                    if (dist > 30.0f) dir_vec(w, 30.0f, w->ay, &pos); /* :1230 */
                    else if (dist < 5.0f) dir_vec(w, 5.0f, w->ay, &pos); /* :1236 */
                    back = player_dv(w, &p, &pos, &dv);             /* :1243-1245 */
                    if (back < 0) return;
                    knock_dir(&p, back, p.hp < 0, &dv);             /* :1247-1286 */
                    put_player(w, &p);
                    break;
                }
            }
        }
        if (fno > 15) w->flg &= ~0x80000u;                          /* :1293 */
        break;
    case 3:
        if (!player(w, &p)) return;
        if (w->ct0 != 0) {                                          /* :1303 */
            p.pos.x += (s->swallow_x - p.pos.x) / (float)w->ct0;    /* :1305-1306 plp->xn/zn */
            p.pos.z += (s->swallow_z - p.pos.z) / (float)w->ct0;
            p.ay += (int16_t)(p.ayp - p.ay) / w->ct0;               /* :1308 */
            w->ct0--;
        }
        if (w->frm_no == 0) {                                       /* :1313 */
            w->mtn_no = 20;
            p.stflg |= 0x1000000u;
            w->mode3++;
        }
        put_player(w, &p);
        if ((w->frm_no == 3342336) || (w->frm_no == 4259840) || (w->frm_no == 6029312)) { /* :1322 (51/65/92) */
            const RECVX_T3_LINK lk = { NULL, 1 };
            recvx_t3_set_blood_effect_burst(lk, 3, 4, NULL, 0);
            if (!vib(w, 1, 11)) return;
        }
        fno = (uint32_t)(w->frm_no / 65536);
        if ((fno > 30) && (fno < 60))      { w->flg |= 0x80000u; s->shake = 2.2f; } /* :1329 */
        else if (fno > 70)                 { w->flg |= 0x80000u; s->shake = 1.2f; } /* :1335 */
        else                                 w->flg &= ~0x80000u;
        break;
    case 4: break;
    default: break;
    }
}
/* en02.c:1353 bhEne02_MV03 — Durchbruch unter dem Spieler. */
static void falling_rock(RECVX_ENEMY_WORK* w);
static void mv03(RECVX_ENEMY_WORK* w)
{
    static const int tim[2][16] = {
        { 26, 46, 56, 26, 46, 56, 46, 46, 56, 46, 46, 56, 56, 56, 56, 26 },
        {  0, 46,  0, 26,  0, 26,  0, 46,  0, 46,  0, 26,  0,  0, 26, 26 }
    };
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC pos, jp, wp, foot, dv, vec;
    int i, back; uint32_t fno; float r;
    switch (w->mode3) {
    case 0:
        SV(w, room_flags, "sys->rm_flg (en02 MV03)");
        en02_sv->room_flags(0x1u, 0u);                              /* :1367 */
        s->ring->mode2 = 4; s->ring->mode3 = 0;                     /* :1369-1370 */
        w->ct0 = 35;                                                /* :1372 */
        if (!vib(w, 1, 12)) return;                                 /* :1374 */
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) w->mode3++;                              /* :1379 */
        break;
    case 2:
        w->flg &= ~0x100000u; w->flg &= ~0x8000u;                   /* :1388-1389 */
        w->mtn_no = 1; w->frm_no = 0; w->mtn_add = 65536;           /* :1391-1394 */
        w->flg |= 0x20u; w->flg |= 0x400000u;                       /* :1396-1397 */
        w->mode3++;
        break;
    case 3:
        if (w->frm_no == 983040) { s->mouth->mode2 = 1; s->mouth->mode3 = 0; } /* :1402 (15) */
        if (!player(w, &p)) return;
        if (w->frm_no == 0) {                                       /* :1408 */
            w->flg |= 0x40u;
            w->mtn_no = 2;
            w->mode3++;
            pos.x = (w->px - (30.0f * njSin((Angle)w->ay))) - p.pos.x; /* :1416-1418 */
            pos.z = (w->pz - (30.0f * njCos((Angle)w->ay))) - p.pos.z;
            pos.y = 0.0f;
            if (!rnd(w, &r)) return;
            if (scalor(pos.x, pos.y, pos.z) < 30.0f) w->ct0 = tim[0][(int)(16.0f * r)]; /* :1420 */
            else                                     w->ct0 = tim[1][(int)(16.0f * r)];
            pos.x = w->px - p.pos.x; pos.z = w->pz - p.pos.z;       /* :1429-1430 */
            w->ayp = recvx_t3_arc_tan2(pos.x, pos.z);               /* :1432 */
            w->ct1 = 16;                                            /* :1434 */
        }
        fno = (uint32_t)(w->frm_no / 65536);
        if ((fno < 10) && !(p.flg & 0x4u)) {                        /* :1437 */
            for (i = 0; i < 9; i++) {                               /* owP[3..11] */
                if (!joint(w, (uint32_t)(3 + i), &jp)) return;
                if (recvx_t3_attack_hit_check(&jp, 12.0f)) {        /* :1448 */
                    p.flg |= 0x10004u; p.stflg |= 0x10000u;         /* :1450-1451 */
                    p.mode0 = 4; p.mode1 = 0; p.mode3 = 0;          /* :1453-1455 */
                    p.hp -= 10; if (p.hp < 0) p.hp = 0;             /* :1457-1462 */
                    wp.x = w->px - (5.0f * njCos((Angle)w->ay));    /* :1464-1466 */
                    wp.y = w->py;
                    wp.z = w->pz + (5.0f * njSin((Angle)w->ay));
                    line_foot(&wp, w->ay, &p.pos, &foot);           /* :1468-1472 */
                    pos = foot;
                    wp.x = w->px; wp.y = w->py; wp.z = w->pz;
                    if (recvx_en03_distance_p2p(&wp, &foot) > 30.0f) { /* :1474 */
                        pos.x = (w->px - (5.0f * njCos((Angle)w->ay))) + (30.0f * -njSin((Angle)w->ay));
                        pos.y = w->py;
                        pos.z = (w->pz + (5.0f * njSin((Angle)w->ay))) + (30.0f * -njCos((Angle)w->ay));
                    } else {
                        vec.x = w->px - p.pos.x; vec.y = w->py; vec.z = w->pz - p.pos.z; /* :1482-1484 */
                        if (recvx_t3_dir_check(w->ay, &vec) != 0) { /* :1486 */
                            pos.x = w->px - (5.0f * njCos((Angle)w->ay));
                            pos.y = w->py;
                            pos.z = w->pz + (5.0f * njSin((Angle)w->ay));
                        }
                    }
                    back = player_dv(w, &p, &pos, &dv);             /* :1494-1496 */
                    if (back < 0) return;
                    if (back) { p.mode2 = 1; p.ayp = recvx_t3_arc_tan2(-dv.x, -dv.z); } /* :1498-1507 */
                    else      { p.mode2 = 0; p.ayp = recvx_t3_arc_tan2(dv.x, dv.z); }
                    put_player(w, &p);
                    break;
                }
            }
        }
        if ((fno > 0) && (fno < 15)) {                              /* :1513 */
            w->flg |= 0x80000u;
            s->shake = (fno < 5) ? 2.2f : 0.6f;
            break;
        }
        w->flg &= ~0x80000u;
        break;
    case 4:
        if (w->type == 1) {                                         /* :1532 */
            BASE(w, wall_type, "bhCheckWallType (en02 MV03)");
            for (i = 0; i < 5; i++) {                               /* owP[7..11] */
                if (!joint(w, (uint32_t)(7 + i), &jp)) return;
                if (w->sv->wall_type(&jp, 0u, 15.0f, 15.0f)) {      /* :1538 */
                    RECVX_ENEMY_VEC wp2 = { w->px, w->py, w->pz };
                    falling_rock(w);
                    w->flg |= 0x80000u;
                    s->shake = 2.2f;
                    recvx_t3_call_effect_se(&wp2, 70418);           /* :1546 */
                    w->mode3++;
                    break;
                }
            }
        }
        /* kein break (:1552): faellt in Fall 5 */
    case 5:
        if (w->ct1 > 0) {                                           /* :1554 */
            if (w->stflg & 0x1u) w->ct1 = 0;
            else { w->ay += (int16_t)(w->ayp - w->ay) / w->ct1; w->ct1--; }
        }
        if ((w->frm_no == 1703936) || (w->frm_no == 3014656)) {     /* :1567 (26/46) */
            if (!player(w, &p)) return;
            pos.x = (w->px - (30.0f * njSin((Angle)w->ay))) - p.pos.x;
            pos.z = (w->pz - (30.0f * njCos((Angle)w->ay))) - p.pos.z;
            pos.y = 0.0f;
            if (scalor(pos.x, pos.y, pos.z) > 30.0f) w->ct0 = 0;
        }
        if (w->ct0-- == 0) { w->mode2 = 4; w->mode3 = 0; }          /* :1579 */
        break;
    default: break;
    }
}
/* en02.c:1595 bhEne02_MV04 — Zurueckziehen mit Schweifschlag. */
static void mv04(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC jp, wp, foot, pos, dv, vec;
    int i, back, djnt; uint32_t fno, n;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x60u;                                            /* :1606 */
        w->mtn_no = 3; w->frm_no = 0;                               /* :1608-1609 */
        w->hokan_count = 10; w->hokan_rate = 0;                     /* :1611-1612 */
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (int32_t)n - 1;                                    /* :1614 */
        w->mode3++;
        /* kein break (:1616) */
    case 1:
        fno = (uint32_t)(w->frm_no / 65536);
        if (fno == 20) { s->mouth->mode2 = 2; s->mouth->mode3 = 0; w->flg &= ~0x40u; } /* :1618 */
        if (w->ct0-- == 0) {                                        /* :1626 */
            w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;
            w->mtn_add = 0;
            w->px -= 40.0f * njSin((Angle)w->ay); w->pz -= 40.0f * njCos((Angle)w->ay);
        }
        if (fno == 14) w->flg &= ~0x400000u;                        /* :1638 */
        if (!player(w, &p)) return;
        if ((fno > 14) && !(p.flg & 0x4u)) {                        /* :1643 */
            for (i = 0; i < 11; i++) {                              /* owP[1..11] */
                float mtx[16];
                BASE(w, joint_matrix, "mlwP->owP[n].mtx (en02 MV04)");
                if (!w->sv->joint_matrix(w, (uint32_t)(1 + i), mtx)) { gap(w, "mlwP->owP[1+i].mtx (en02 MV04)"); return; }
                jp.x = mtx[12]; jp.y = mtx[13]; jp.z = mtx[14];
                if (((mtx[13] - 6.0f) >= p.pos.y) && recvx_t3_attack_hit_check(&jp, 12.0f)) { /* :1649 */
                    if (i > 9) {                                    /* :1651 */
                        p.hp -= 30;
                        if (!player_djnt(w, &djnt)) return;
                        { const RECVX_T3_LINK lk = { NULL, 1 }; recvx_t3_set_blood_effect_burst(lk, 3, djnt, NULL, 0); }
                    } else {
                        p.hp -= 10; if (p.hp < 0) p.hp = 0;
                    }
                    wp.x = w->px; wp.y = w->py; wp.z = w->pz;
                    line_foot(&wp, w->ay, &p.pos, &foot);           /* :1667-1675 */
                    pos = foot;
                    if (recvx_en03_distance_p2p(&wp, &foot) > 30.0f) dir_vec(w, 30.0f, w->ay, &pos); /* :1677 */
                    else {
                        vec.x = w->px - p.pos.x; vec.y = w->py; vec.z = w->pz - p.pos.z; /* :1689-1691 */
                        if (recvx_t3_dir_check(w->ay, &vec) != 0) pos = wp; /* :1693 */
                    }
                    back = player_dv(w, &p, &pos, &dv);             /* :1704-1706 */
                    if (back < 0) return;
                    knock_dir(&p, back, p.hp < 0, &dv);             /* :1708-1747 */
                    put_player(w, &p);
                    break;
                }
            }
        }
        if (fno > 21) {                                             /* :1754 */
            w->flg |= 0x80000u;
            s->shake = (fno < 30) ? 2.2f : 1.2f;
        }
        break;
    default: break;
    }
}
/* en02.c:1777 bhEne02_MV05 — Hieb-Ausklang. */
static void mv05(RECVX_ENEMY_WORK* w)
{
    uint32_t n; float r;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x60u;                                            /* :1782 */
        if (!rnd(w, &r)) return;
        w->mtn_no = (r > 0.5f) ? 4u : 5u;                           /* :1784 */
        w->frm_no = 0; w->mtn_add = 65536;                          /* :1785-1787 */
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (int32_t)n - 1;                                    /* :1789 */
        w->mode3++;
        /* kein break (:1791) */
    case 1:
        if (w->ct0-- == 0) { w->mode0 = 1; w->mode1 = 0; w->mode2 = 4; w->mode3 = 0; } /* :1793 */
        break;
    default: break;
    }
}
static void damage_init(RECVX_ENEMY_WORK* w);
static void move(RECVX_ENEMY_WORK* w)
{
    if (w->mode1 == 1) brain(w);                                    /* :952 */
    switch (w->mode2) {                                             /* :957 bhEne02_MoveMode2[6] */
    case 0: case 1: mv00(w); break;
    case 2: mv02(w); break;
    case 3: mv03(w); break;
    case 4: mv04(w); break;
    case 5: mv05(w); break;
    default: gap(w, "bhEne02_MoveMode2 index"); break;
    }
    if ((w->flg & 0x4u) && !(w->flg & 0x2u)) {                      /* :959 */
        w->flg &= ~0x4u;
        damage_init(w);
    }
}

/* --- Damage / Die (en02.c:1812-2250) --------------------------------------- */
static void hit_mark(RECVX_ENEMY_WORK* w);
static void die(RECVX_ENEMY_WORK* w);
static void dg01(RECVX_ENEMY_WORK* w)
{
    uint32_t n;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x10u;                                           /* :1873 */
        w->mtn_no = 6; w->frm_no = 0;                               /* :1875-1876 */
        w->hokan_count = 20; w->hokan_rate = 0;                     /* :1878-1879 */
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (int32_t)n - 2;                                    /* :1881 */
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) {                                        /* :1886 */
            w->mode0 = 1; w->mode1 = 0; w->mode2 = 4; w->mode3 = 0;
            w->flg &= ~0x4u; w->flg |= 0x10u;
        }
        break;
    default: break;
    }
}
static void damage(RECVX_ENEMY_WORK* w)
{
    if (w->flg & 0x4u) {                                            /* :1814 */
        w->flg &= ~0x4u;
        recvx_t3_calc_damage(w, recvx_en02_comb_wep, recvx_en02_comb_joint); /* :1818 */
        if ((w->wpnr_no == 16) && !(w->flg2 & 0x4u) && (w->comb_pnt != 1)) goto label; /* :1820 */
        w->hp -= w->total_dam;                                      /* :1825 */
        if ((w->wpnr_no == 17) && !(w->flg2 & 0x4u)) goto label;    /* :1827 */
        hit_mark(w);                                                /* :1832 */
    }
    if ((w->flg & 0x400000u) && (w->hp < 0)) {                      /* :1835 */
        w->flg |= 0x2u; w->flg &= ~0x20u;
        w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
        w->mode2 = (w->type == 0) ? 0 : 1;
        die(w);
        return;
    }
label:
    switch (w->mode2) {                                             /* :1857 bhEne02_DamageMode2[2] */
    case 0: break;                                                  /* DG00 leer */
    case 1: dg01(w); break;
    default: gap(w, "bhEne02_DamageMode2 index"); break;
    }
}
static void damage_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w);
    recvx_t3_calc_damage(w, recvx_en02_comb_wep, recvx_en02_comb_joint); /* :2946 */
    if ((w->wpnr_no != 16) || (w->flg2 & 0x4u) || (w->comb_pnt == 1)) { /* :2948 */
        w->hp -= w->total_dam;
        if ((w->wpnr_no != 17) || (w->flg2 & 0x4u)) {               /* :2952 */
            hit_mark(w);
            if (w->flg & 0x400000u) {                               /* :2956 */
                if (w->hp < 0) {
                    w->flg |= 0x2u; w->flg &= ~0x20u;
                    w->mode0 = 4; w->mode1 = 0; w->mode3 = 0;
                    w->mode2 = (w->type == 0) ? 0 : 1;
                    if (w->type == 1) {                             /* :2978 */
                        s->mouth->mdflg |= 0x1u;
                        s->mouth->flg &= ~0x48u;
                    }
                } else if (w->total_dam > 50) {                     /* :2985 */
                    w->mode0 = 3; w->mode1 = 0; w->mode2 = 1; w->mode3 = 0;
                }
            }
        }
    }
}
/* en02.c:1908 bhEne02_DD00 — Tod an der Oberflaeche. */
static void dd00(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC pos, probe, dv, jp, wp; RECVX_ENEMY_ATR atr;
    uint32_t n; int back, i;
    switch (w->mode3) {
    case 0:
        w->flg |= 0x40u; w->flg &= ~0x10u;                          /* :1916-1917 */
        w->mtn_no = 11; w->frm_no = 0;                              /* :1919-1920 */
        w->hokan_count = 20; w->hokan_rate = 0;                     /* :1922-1923 */
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (int32_t)n - 2;                                    /* :1925 */
        w->ct1 = 0; w->ct2 = 0;
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) {                                        /* :1932 */
            w->mtn_add = 0;
            w->flg &= ~0x48u;
            w->mode3++;
        }
        if (w->frm_no < 10485760) {                                 /* :1940 (160): Stapel-Dummy bhCheckWall */
            uint32_t stflg = 0u;                                    /* dmy_ene.flg 0x19, stflg 0, ar 20, ah 1 */
            if (w->frm_no > 7864320) dir_vec(w, 20.0f, w->ay, &pos);          /* :1945 (120) */
            else                     dir_vec(w, 20.0f, w->ay + 22755, &pos);  /* :1952 */
            probe = pos;
            SV(w, wall_probe, "bhCheckWall(&dmy_ene) (en02.c:1962)");
            if (en02_sv->wall_probe(0x19u, &stflg, &probe, 20.0f, 1.0f) && (stflg & 0x1u)) { /* :1964 */
                RECVX_ENEMY_VEC d = { pos.x - probe.x, pos.y - probe.y, pos.z - probe.z }, rl; /* :1966 njSubVector */
                rot_y(-w->ay, &d, &rl);                             /* :1968-1972 */
                if (w->frm_no > 7864320) {                          /* :1974 */
                    if ((rl.x > 0.0f) && (w->ct1 != -1)) { w->ay += 546; w->ct1 = 1; }
                    if ((rl.x < 0.0f) && (w->ct1 != 1))  { w->ay -= 546; w->ct1 = -1; }
                } else {
                    if ((rl.x < 0.0f) && (w->ct2 != -1)) { w->ay += 546; w->ct2 = 1; }
                    if ((rl.x > 0.0f) && (w->ct2 != 1))  { w->ay -= 546; w->ct2 = -1; }
                }
            }
        }
        BASE(w, mobile_wall_get, "sys->mwalp[n] (en02 DD00)");
        BASE(w, mobile_wall_set, "sys->mwalp[n] (en02 DD00)");
        if (w->frm_no > 10354688) {                                 /* :2007 (158) */
            if (!joint(w, 10u, &jp)) return;
            if (!w->sv->mobile_wall_get(s->wall_index, &atr)) { gap(w, "sys->mwalp[n] lesen (en02 DD00)"); return; }
            atr.flg = 1;
            atr.px = jp.x; atr.py = w->py; atr.pz = jp.z;
            w->sv->mobile_wall_set(s->wall_index, &atr);
        }
        if (!player(w, &p)) return;
        if ((w->frm_no > 10354688) && (w->frm_no < 12189696)) {     /* :2018 (158..186) */
            RECVX_ENEMY_VEC v = { 0.0f, 0.0f, -10.0f };
            s->car_target = 18.0f;                                  /* :2020 */
            rot_y(w->ay, &v, &pos);                                 /* :2022-2030 */
            w->aox = pos.x; w->aoy = pos.y; w->aoz = pos.z;          /* :2032-2034 */
            pos.x += w->px; pos.y += w->py; pos.z += w->pz;          /* :2036 njAddVector */
            if (!(p.flg & 0x4u) && recvx_t3_attack_hit_check(&pos, 20.0f)) { /* :2038 */
                p.flg |= 0x10004u; p.stflg |= 0x10000u;
                p.mode0 = 4; p.mode1 = 0; p.mode3 = 0;
                p.hp -= 10; if (p.hp < 0) p.hp = 0;
                back = player_dv(w, &p, &pos, &dv);                 /* :2053-2055 */
                if (back < 0) return;
                if (back) { p.mode2 = 1; p.ayp = recvx_t3_arc_tan2(-dv.x, -dv.z); } /* :2057-2068 */
                else      { p.mode2 = 0; p.ayp = recvx_t3_arc_tan2(dv.x, dv.z); }
                put_player(w, &p);
                break;
            }
        }
        if ((((w->frm_no > 11010048) && (w->frm_no < 13107200)) || ((w->frm_no > 15073280) && (w->frm_no < 16384000)) ||
             ((w->frm_no > 17432576) && (w->frm_no < 19005440)) || (w->frm_no > 19398656)) && !(p.flg & 0x4u)) { /* :2074 */
            for (i = 0; i < 3; i++) {                               /* owP[5..7] */
                if (!joint(w, (uint32_t)(5 + i), &jp)) return;
                if (recvx_t3_attack_hit_check(&jp, 12.0f)) {        /* :2083 */
                    p.flg |= 0x10004u; p.stflg |= 0x10000u;
                    p.mode0 = 4; p.mode1 = 0; p.mode3 = 0;
                    p.hp -= 10; if (p.hp < 0) p.hp = 0;
                    dir_vec(w, 10.0f, w->ay, &pos);                 /* :2098-2100 */
                    back = player_dv(w, &p, &pos, &dv);
                    if (back < 0) return;
                    if (back) { p.mode2 = 1; p.ayp = recvx_t3_arc_tan2(-dv.x, -dv.z); }
                    else      { p.mode2 = 0; p.ayp = recvx_t3_arc_tan2(dv.x, dv.z); }
                    put_player(w, &p);
                    break;
                }
            }
        }
        if (w->frm_no > 21626880) {                                 /* :2125 (330) */
            s->car_target = 10.0f;
            w->aox *= 0.7f; w->aoy *= 0.7f; w->aoz *= 0.7f;
        }
        if (w->frm_no > 22937600) {                                 /* :2134 (350) */
            w->flg |= 0x100000u;
            if (!w->sv->mobile_wall_get(s->wall_index, &atr)) { gap(w, "sys->mwalp[n] lesen (en02 DD00)"); return; }
            atr.flg = 0;
            w->sv->mobile_wall_set(s->wall_index, &atr);
        }
        if (w->frm_no == 21626880) { s->mouth->mode2 = 2; s->mouth->mode3 = 0; } /* :2141 */
        if ((w->frm_no > 10878976) && (w->frm_no < 11796480)) {     /* :2147 (166..180) */
            w->flg |= 0x80000u;
            s->shake = (w->frm_no < 11141120) ? 2.2f : 0.6f;        /* 170 */
            break;
        }
        if ((w->frm_no > 17039360) && (w->frm_no < 17891328)) { w->flg |= 0x80000u; s->shake = 0.6f; break; } /* :2161 */
        if ((w->frm_no > 18481152) && (w->frm_no < 19660800)) { w->flg |= 0x80000u; s->shake = 0.7f; break; } /* :2169 */
        if ((w->frm_no > 20971520) && (w->frm_no < 23134208)) { w->flg |= 0x80000u; s->shake = 0.5f; break; } /* :2177 */
        w->flg &= ~0x80000u;
        break;
    default: break;
    }
    (void)wp;
}
/* en02.c:2202 bhEne02_DD01 — Tod im Boden. */
static void dd01(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); uint32_t n, fno;
    switch (w->mode3) {
    case 0:
        w->flg &= ~0x10u;                                           /* :2207 */
        w->mtn_no = 8; w->frm_no = 0;                               /* :2209-2210 */
        w->hokan_count = 20; w->hokan_rate = 0;                     /* :2212-2213 */
        if (!frames(w, w->mtn_no, &n)) return;
        w->ct0 = (int32_t)n - 2;                                    /* :2215 */
        s->mouth->mode2 = 2; s->mouth->mode3 = 0;                   /* :2217-2218 */
        w->mode3++;
        break;
    case 1:
        if (w->ct0-- == 0) { w->mtn_add = 0; w->mode3++; }          /* :2223 */
        fno = (uint32_t)(w->frm_no / 65536);
        if (fno == 86)  { w->flg |= 0x80000u; s->shake = 5.0f; break; } /* :2230 */
        if (fno == 117) { w->flg |= 0x80000u; s->shake = 4.2f; break; } /* :2238 */
        w->flg &= ~0x80000u;
        break;
    default: break;
    }
}
static void die(RECVX_ENEMY_WORK* w)
{
    switch (w->mode2) {                                             /* :1904 bhEne02_DeadMode2[2] */
    case 0: dd00(w); break;
    case 1: dd01(w); break;
    default: gap(w, "bhEne02_DeadMode2 index"); break;
    }
}

/* --- PlayerControl (en02.c:2661) ------------------------------------------ */
static const int pc_mtn[3][8] = {
    { 21, 22, 23, 24, 25, 26, 28, 27 },
    { 29, 30, 31, 32, 33, 34, 36, 35 },
    { 29, 30, 31, 32, 33, 34, 36, 35 }
};
static const RECVX_ENEMY_VEC* pc_trans(uint32_t id, int k)
{
    static const RECVX_ENEMY_VEC* const t[3][3] = {
        { recvx_cler_042, recvx_cler_043, recvx_cler_045 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 },
        { recvx_cher_060, recvx_cher_061, recvx_cher_063 }
    };
    return t[id][k];
}
static void library(RECVX_ENEMY_WORK* w, uint32_t source)
{
    SV(w, player_library, "bhEne02_PlayerControl: plp->mnwP");
    en02_sv->player_library(w, source);
}
/* bhEne_AddNullTransDir(plp, ayp, trans) / bhEne_AddNullTrans(plp, trans):
   der Helfer liest und schreibt den Spielerwork selbst. */
static void null_trans_dir(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, int k)
{
    const RECVX_T3_LINK lk = { NULL, 1 };
    put_player(w, p);
    recvx_t3_add_null_trans_dir(lk, p->ayp, pc_trans(p->ply_id, k));
    player(w, p);
}
static void null_trans(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p, int k)
{
    const RECVX_T3_LINK lk = { NULL, 1 };
    put_player(w, p);
    recvx_t3_add_null_trans(lk, pc_trans(p->ply_id, k));
    player(w, p);
}
static void player_control(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p; uint32_t n;
    if (!player(w, &p)) return;
    if (p.mode0 != 4 && p.mode0 != 6) return;                       /* :2678/:2802 */
    if (p.ply_id > 2) { gap(w, "bhEne02_PlayerControl: mtn[sys->ply_id] ausserhalb"); return; }
    if (p.mode0 == 4) {                                             /* :2678 Niederschlag */
        if (p.mode2 == 0 || p.mode2 == 1) {
            switch (p.mode3) {
            case 0:
                p.flg &= ~0x40000u; p.flg2 |= 0x1u;                 /* :2687-2688 */
                p.mtn_enemy = 1; p.mtn_enemy_owner = w->id; p.mtn_enemy_work = w; /* :2690 plp->mnwP = epw->mnwP */
                p.mtn_no = (uint32_t)pc_mtn[p.ply_id][(p.mode2 == 0) ? 0 : 1]; /* :2692-2699 */
                p.frm_no = 0;                                       /* :2701 */
                p.hokan_count = 3; p.hokan_rate = 32768;            /* :2703-2704 */
                p.mtn_add = 65536;                                  /* :2706 */
                p.mode3++;
                p.ct0 = 8;                                          /* :2710 */
                put_player(w, &p); library(w, 1u);
                recvx_t3_call_player_voice(2);                      /* :2712 */
                if (!vib(w, 1, 11)) return;                         /* :2714 */
                if (!player(w, &p)) return;
                break;
            case 1:
                null_trans_dir(w, &p, (p.mode2 == 0) ? 0 : 1);      /* :2717-2724 */
                if (p.ct0 != 0) {                                   /* :2726 */
                    p.ay += (int16_t)(p.ayp - p.ay) / p.ct0;
                    p.ct0--;
                }
                if (p.frm_no == 0) {                                /* :2733 */
                    if (p.mode2 == 0) { p.mtn_no = (uint32_t)pc_mtn[p.ply_id][2]; p.flg |= 0xC0000u; }
                    else                p.mtn_no = (uint32_t)pc_mtn[p.ply_id][3];
                    p.mode3++;
                }
                break;
            case 2:
                if (p.mode2 == 0) {                                 /* :2751 */
                    if ((p.frm_no / 65536) == 19) p.flg &= ~0x80000u;
                    if ((p.ply_id == 0) && ((p.frm_no / 65536) == 41)) p.flg |= 0x80000u;
                } else if (p.mtn_no == (uint32_t)pc_mtn[p.ply_id][3]) null_trans(w, &p, 2); /* :2765 */
                if (p.frm_no == 0) {                                /* :2771 */
                    p.mtn_enemy = 0; p.mtn_enemy_work = NULL;       /* plp->mnwP = plp->mnwPb */
                    p.flg &= ~0x10004u; p.flg2 &= ~0x1u;            /* :2775-2776 */
                    p.flg |= 0x8u;                                  /* :2778 */
                    p.at_flg = 0;                                   /* :2780 */
                    p.stflg &= ~0x10000u;                           /* :2782 */
                    p.mode0 = 1; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0; /* :2784 *(int*)&plp->mode0 = 1 */
                    put_player(w, &p); library(w, 0u);
                    if (!player(w, &p)) return;
                }
                break;
            default: break;
            }
        }
        sand_effect_player(w, &p);                                  /* :2793 */
        p.flg |= 0x200000u;                                         /* :2795 */
        put_player(w, &p);
        return;
    }
    /* mode0 6: Tod (:2797) */
    if (p.mode2 == 0 || p.mode2 == 1) {                             /* verschluckt */
        switch (p.mode3) {
        case 0:
            p.flg &= ~0x40000u;                                     /* :2805 */
            p.mtn_enemy = 1; p.mtn_enemy_owner = w->id; p.mtn_enemy_work = w; /* :2807 */
            p.mtn_no = (uint32_t)pc_mtn[p.ply_id][(p.mode2 != 0) ? 4 : 5]; /* :2809-2816 */
            p.frm_no = 0;
            p.hokan_count = 3; p.hokan_rate = 32768;
            p.mtn_add = 65536;
            put_player(w, &p); library(w, 1u);
            BASE(w, player_motion_frames, "bhEne02_PlayerControl: plp->mnwP[mtn_no].frm_num");
            if (!(n = w->sv->player_motion_frames(p.mtn_no, 1))) { gap(w, "bhEne02_PlayerControl: mnwP[mtn_no] fehlt"); return; }
            p.ct0 = (int32_t)n - 2;                                 /* :2825 */
            p.mode3++;
            put_player(w, &p);
            recvx_t3_call_player_voice(2);                          /* :2829 */
            if (!vib(w, 1, 11)) return;                             /* :2831 */
            if (!player(w, &p)) return;
            break;
        case 1:
            if (p.ct0-- == 0) { p.mtn_add = 0; p.flg |= 0x2u; }     /* :2834 */
            break;
        default: break;
        }
    } else if (p.mode2 == 2 || p.mode2 == 3) {                      /* :2844 Niederschlag mit Tod */
        switch (p.mode3) {
        case 0:
            p.flg &= ~0x40000u; p.flg2 |= 0x1u;                     /* :2849-2850 */
            p.mtn_enemy = 1; p.mtn_enemy_owner = w->id; p.mtn_enemy_work = w; /* :2852 */
            p.mtn_no = (uint32_t)pc_mtn[p.ply_id][(p.mode2 == 2) ? 0 : 1]; /* :2854-2861 */
            p.frm_no = 0;
            p.hokan_count = 3; p.hokan_rate = 32768;
            p.mtn_add = 65536;
            p.mode3++;
            p.ct0 = 8;                                              /* :2872 */
            put_player(w, &p); library(w, 1u);
            recvx_t3_call_player_voice(1);                          /* :2874 */
            if (!vib(w, 1, 11)) return;                             /* :2876 */
            if (!player(w, &p)) return;
            break;
        case 1:
            null_trans_dir(w, &p, (p.mode2 == 2) ? 0 : 1);          /* :2879-2886 */
            if (p.ct0 != 0) {                                       /* :2888 */
                p.ay += (int16_t)(p.ayp - p.ay) / p.ct0;
                p.ct0--;
            }
            if (p.frm_no == 0) {                                    /* :2895 */
                p.mtn_no = (uint32_t)pc_mtn[p.ply_id][(p.mode2 == 2) ? 6 : 7];
                put_player(w, &p);
                BASE(w, player_motion_frames, "bhEne02_PlayerControl: plp->mnwP[mtn_no].frm_num");
                if (!(n = w->sv->player_motion_frames(p.mtn_no, 1))) { gap(w, "bhEne02_PlayerControl: mnwP[mtn_no] fehlt"); return; }
                p.ct0 = (int32_t)n - 2;                             /* :2906 */
                p.mode3++;
            }
            break;
        case 2:
            if (p.ct0-- == 0) { p.mtn_add = 0; p.ct0 = 45; p.mode3++; } /* :2913 */
            break;
        case 3:
            if (p.ct0-- == 0) p.flg |= 0x2u;                        /* :2923 */
            break;
        default: break;
        }
    }
    sand_effect_player(w, &p);                                      /* :2935 */
    p.flg |= 0x200000u;                                             /* :2937 */
    put_player(w, &p);
}

/* --- CheckWall / Camera / Warp / FallingRock / CallSE / HitMark ------------- */
static void check_wall(RECVX_ENEMY_WORK* w)
{
    float dx, dz, ar;
    if (!(w->flg & 0x10u)) return;                                  /* :3048 */
    BASE(w, check_wall, "bhCheckWall (en02)");
    w->stflg &= ~0x1u;                                              /* :3050 */
    dx = -40.0f * njSin((Angle)w->ay); dz = -40.0f * njCos((Angle)w->ay); /* :3052-3053 */
    ar = w->ar;
    w->px += dx; w->pz += dz;                                       /* :3057-3058 */
    w->ar = 5.0f;                                                   /* :3060 */
    w->sv->check_wall(w);                                           /* :3062 */
    w->px -= dx; w->pz -= dz;                                       /* :3064-3065 */
    w->ar = ar;                                                     /* :3067 */
    w->sv->check_wall(w);                                           /* :3069 */
}
static void camera_control(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); float x, y, z, r;
    if (w->mode0 == 5) return;                                      /* :3076 */
    SV(w, camera_offset, "cam.ofx/ofy/ofz (en02)");
    if (w->flg & 0x80000u) {                                        /* :3081 */
        if (!rnd(w, &r)) return; x = (s->shake * r) - (s->shake / 2.0f);
        if (!rnd(w, &r)) return; y = (s->shake * r) - (s->shake / 2.0f);
        if (!rnd(w, &r)) return; z = (s->shake * r) - (s->shake / 2.0f);
        en02_sv->camera_offset(x, y, z);
    } else if (s->shake > 0.01f) {                                  /* :3087 */
        if (!rnd(w, &r)) return; x = (s->shake * r) - (s->shake / 2.0f);
        if (!rnd(w, &r)) return; y = (s->shake * r) - (s->shake / 2.0f);
        if (!rnd(w, &r)) return; z = (s->shake * r) - (s->shake / 2.0f);
        en02_sv->camera_offset(x, y, z);
        s->shake *= 0.9f;                                           /* :3093 */
    } else {
        en02_sv->camera_offset(0.0f, 0.0f, 0.0f);                   /* :3097 */
    }
}
static void warp_check(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); RECVX_ENEMY_PLAYER p; RECVX_ENEMY_ATR atr; int32_t index; int ang;
    if (!player(w, &p)) return;
    SV(w, floor_enemy, "bhCheckFloorEnemy (en02 WarpCheck)");
    if (en02_sv->floor_enemy(w->flr_no, p.pos.x, p.pos.z, &atr, &index) &&
        (index != s->warp_floor) && (atr.prm0 == 2)) {              /* :3108 hp != NULL && hp != gespeichert && prm0 == 2 */
        s->warp_floor = index;                                      /* :3110 */
        if (atr.prm1 == 0) {                                        /* :3112 */
            w->px = p.pos.x - (40.0f * njSin((Angle)p.ay));
            w->pz = p.pos.z - (40.0f * njCos((Angle)p.ay));
            w->ay = p.ay + 32768;
        } else if (atr.prm1 == 1) {                                 /* :3119 */
            ang = (int)atr.prm2 * 1820;
            w->px = p.pos.x - (40.0f * njSin((Angle)ang));
            w->pz = p.pos.z - (40.0f * njCos((Angle)ang));
            w->ay = ang + 32768;
        }
    } else {
        s->warp_floor = -1;                                         /* :3131 EXP0_I(116) = 0 */
    }
}
static void falling_rock(RECVX_ENEMY_WORK* w)
{
    RECVX_EN02_STATE* s = st(w); int i, num; float dx, dy, dz, r;
    SV(w, child_model, "bhEne02_FallingRock: epp->mlwP = &epw->mdl[k]");
    if (!rnd(w, &r)) return;
    num = (int)(4.0f * r) + 12;                                     /* :3143 */
    dx = w->px - (30.0f * njSin((Angle)w->ay));                     /* :3145-3147 */
    dy = w->py + 60.0f;
    dz = w->pz - (30.0f * njCos((Angle)w->ay));
    for (i = 0; i < (int)RECVX_EN02_ROCKS; i++) {
        RECVX_ENEMY_WORK* c = s->rocks[i];
        if (c->mode2 != 0) continue;                                /* :3151 */
        c->mode2 = 3; c->mode3 = 0;                                 /* :3153-3154 */
        if (!rnd(w, &r)) return; c->px = (dx + (80.0f * r)) - 40.0f; /* :3156 */
        c->py = dy;
        if (!rnd(w, &r)) return; c->pz = (dz + (80.0f * r)) - 40.0f; /* :3158 */
        if (!rnd(w, &r)) return;
        if (!en02_sv->child_model(c, w, (uint32_t)((int)(3.0f * r) + 1))) gap(w, "bhEne02_FallingRock: mdl[k] fehlt"); /* :3160 */
        if (--num == 0) break;                                      /* :3162 */
    }
}
static void call_se(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC pos = { w->px, w->py, w->pz }; int fno;
    if (w->mtn_room) return;                                        /* :3175 mnwP != mnwPb */
    fno = w->frm_no / 65536;
    switch (w->mtn_no) {
    case 1:  if (fno == 7) recvx_t3_call_se(w, &pos, 70403); break;
    case 2:
        if (fno == 6)  recvx_t3_call_se(w, &pos, 16847622);
        if (fno == 22) recvx_t3_call_se(w, &pos, 70405);
        if (fno == 52) recvx_t3_call_se(w, &pos, 70405);
        break;
    case 3:  if (fno == 23) recvx_t3_call_effect_se(&pos, 70404); break;
    case 6:  if (fno == 1) recvx_t3_call_se(w, &pos, 16782088); break;
    case 8:
        if (fno == 1) recvx_t3_call_se(w, &pos, 16782089);
        if ((fno == 86) || (fno == 117)) recvx_t3_call_se(w, &pos, 4874);
        break;
    case 11:
        if (fno == 1)   recvx_t3_call_se(w, &pos, 16847627);
        if (fno == 167) recvx_t3_call_se(w, &pos, 4874);
        if (fno == 263) recvx_t3_call_se(w, &pos, 70421);
        if (fno == 284) recvx_t3_call_effect_se(&pos, 70421);
        if (fno == 320) recvx_t3_call_se(w, &pos, 70404);
        break;
    case 14: if (fno == 1) recvx_t3_call_se(w, &pos, 70403); break;
    case 16: if (fno == 9) recvx_t3_call_effect_se(&pos, 70404); break;
    case 17:
        if (fno == 1)  recvx_t3_call_effect_se(&pos, 70412);
        if (fno == 97) recvx_t3_call_effect_se(&pos, 70413);
        break;
    default: break;
    }
}
/* en02.c:3291 bhEne02_HitMark. Zufallsreihenfolge je Versatz x, y, z. */
static void hit_mark(RECVX_ENEMY_WORK* w)
{
    const RECVX_EN02_BLOOD* blp; const RECVX_EN02_DMG_REACT* rc; RECVX_ENEMY_VEC ofp; int range, i; float r;
    if (w->djnt_no < 0 || w->djnt_no >= 24) { gap(w, "bhEne02_HitMark: BloodTbl[djnt_no] ausserhalb"); return; }
    if (w->wpnr_no < 0 || w->wpnr_no >= 21) { gap(w, "bhEne02_HitMark: DmgReact[wpnr_no] ausserhalb"); return; }
    blp = &recvx_en02_blood_tbl[w->djnt_no];
    rc  = &recvx_en02_dmg_react[w->wpnr_no];
    range = 0;                                                      /* :3299-3313 */
    if (w->comb_flg & 0x10u) range = 0;
    if (w->comb_flg & 0x20u) range = 1;
    if (w->comb_flg & 0x40u) range = 2;
#define OFS() do { \
        if (!rnd(w, &r)) return; ofp.x = blp->ofp.x + ((blp->rx * r) - (blp->rx / 2.0f)); \
        ofp.y = (w->comb_flg & 0x8u) ? blp->ofp.y : -blp->ofp.y; \
        if (!rnd(w, &r)) return; ofp.y += (blp->ry * r) - (blp->ry / 2.0f); \
        if (!rnd(w, &r)) return; ofp.z = blp->ofp.z + ((blp->rz * r) - (blp->rz / 2.0f)); \
    } while (0)
    if (rc->type[range] >= 0) {                                     /* :3315 */
        RECVX_T3_LINK lk = { w, 0 }; RECVX_ENEMY_VEC dp = { w->dpx, w->dpy, w->dpz };
        OFS();
        recvx_t3_set_blood_effect_burst(lk, rc->type[range], w->djnt_no, &ofp, 0); /* :3326 */
        switch (w->wpnr_no) {                                       /* :3328 */
        case 10: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20:
            recvx_t3_set_blood_effect_burst(lk, rc->type[range], w->djnt_no, &dp, 1);
            break;
        default:
            recvx_t3_set_blood_effect_burst(lk, rc->type[range], w->djnt_no, &ofp, 0);
            break;
        }
        if (rc->bloodstain[range] != 0) recvx_t3_set_bloodstain(w, 0, w->djnt_no, &ofp); /* :3345 */
    }
    if ((rc->exef & 0x1) && (blp->flg == 0)) {                      /* :3351 */
        for (i = 0; i < 4; i++) {
            float size, len;
            OFS();
            if (!rnd(w, &size) || !rnd(w, &len)) return;
            recvx_t3_set_fire_effect(w, w->djnt_no, &ofp, 1.0f + size, (int)(40.0f * len) + 20); /* :3364 */
        }
    }
    if (rc->exef & 0x2) {                                           /* :3368 */
        OFS();
        recvx_t3_set_acid_effect(w, w->djnt_no, &ofp, 2.0f);        /* :3379 */
    }
#undef OFS
}

/* --- Einstieg bhEne02 (en02.c:714) ---------------------------------------- */
void recvx_en02(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    switch (w->mode0) {                                             /* :716 bhEne02_Mode0[6] */
    case 0: init(w); break;
    case 1: move(w); break;
    case 2: break;                                                  /* bhEne02_Nage leer (:1806) */
    case 3: damage(w); break;
    case 4: die(w); break;
    case 5: if (w->sv && w->sv->event) w->sv->event(w); else gap(w, "bhEne_Event (en02)"); break;
    default: gap(w, "bhEne02_Mode0 index"); return;
    }
    if (!(w->flg & 0x100000u)) sand_effect_mtn(w);                  /* :718-721 */
    BASE(w, set_motion, "bhSetMotion (en02)");
    w->sv->set_motion(w, w->mtn_add, w->mtn_md);                    /* :723 */
    check_wall(w);                                                  /* :725 */
    BASE(w, calc_model, "bhCalcModel (en02)");
    w->sv->calc_model(w);                                           /* :727 */
    if (w->flg & 0x100000u) w->car = 0.0f;                          /* :729-736 */
    else if (w->exp0) w->car += (st(w)->car_target - w->car) / 16.0f;
    BASE(w, weapon_atr, "bhEne_SetWeponAtr (en02)");
    w->sv->weapon_atr(w, 11, 3, 10.0f);                             /* :738 */
    if (w->exp0) { player_control(w); camera_control(w); }           /* :740-741 */
    call_se(w);                                                     /* :743 */
}

/* ===========================================================================
   bhEne02sub (en02sub.c) — Kindwork Identitaet 31: type 0 Maul (Morph
   mdl[5]/mdl[6]), type 1 Ring (mdl[4]), type 2 Felsen (mdl[1..3]).
   =========================================================================== */
static const RECVX_T3_PKEY sub_open[4]  = { { 0, 0.0f }, { 6, 900.0f }, { 10, 1000.0f }, { 999, 1000.0f } }; /* en02sub.c:12 */
static const RECVX_T3_PKEY sub_close[3] = { { 0, 1000.0f }, { 10, 0.0f }, { 999, 0.0f } };                   /* en02sub.c:19 */

static void sub_init(RECVX_ENEMY_WORK* w)
{
    w->mdflg |= 0x4u; w->mdflg |= 0x1u;                             /* en02sub.c:48-49 */
    w->flg2 |= 0x1u;                                                /* :51 */
    if (w->type == 0) { w->car = 10.0f; w->cah = 20.0f; }           /* :53-57 */
    w->mode0 = 1; w->mode1 = 0; w->mode2 = 0; w->mode3 = 0;         /* :59-62 */
}
static void sub_mv01(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* ep = w->lkwkp;
    switch (w->mode3) {
    case 0:
        if (!ep) { gap(w, "bhEne02sub_MV01: lkwkp NULL"); return; }
        w->flg |= 0x48u;                                            /* :87 */
        w->mdflg |= 0x2u; w->mdflg &= ~0x1u;                        /* :89-90 */
        w->ct0 = 0;
        w->px = ep->px; w->py = ep->py; w->pz = ep->pz;             /* :96-98 */
        w->ay = (ep->ay + 32767) + 1;                               /* :100 */
        w->car = 0.1f;                                              /* :102 */
        w->mode3++;
        /* kein break (:104) */
    case 1:
        w->shp_ct = recvx_t3_get_shape_cnt(sub_open, 4, w->ct0);    /* :106 */
        if (w->ct0 > 30) w->mode2 = 0;                              /* :108 */
        w->car += (10.0f - w->car) / 16.0f;                         /* :113 */
        w->ct0++;
        break;
    default: break;
    }
}
static void sub_mv02(RECVX_ENEMY_WORK* w)
{
    switch (w->mode3) {
    case 0:
        w->mdflg |= 0x2u; w->mdflg &= ~0x1u;                        /* :126-127 */
        w->ct0 = 0;
        w->car = 10.0f;                                             /* :131 */
        w->mode3++;
        /* kein break (:133) */
    case 1:
        w->shp_ct = recvx_t3_get_shape_cnt(sub_close, 3, w->ct0);   /* :135 */
        if (w->ct0 > 10) { w->mdflg |= 0x1u; w->flg &= ~0x48u; w->mode2 = 0; } /* :137 */
        w->car += -w->car / 16.0f;                                  /* :145 */
        w->ct0++;
        break;
    default: break;
    }
}
static void sub_mv03(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* ep = w->lkwkp; RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC pos, wp; float r;
    if (!ep) { gap(w, "bhEne02sub_MV03: lkwkp NULL"); return; }
    switch (w->mode3) {
    case 0:
        w->mdflg &= ~0x1u;                                          /* :162 */
        w->yn = -2.0f;                                              /* :164 */
        if (!rnd(w, &r)) return; w->ct0 = (int)(60.0f * r);         /* :166 */
        w->mode3++;
        /* kein break (:168) */
    case 1:
        if (w->ct0-- == 0) w->mode3++;                              /* :170 */
        break;
    case 2:
        if (!player(w, &p)) return;
        wp.x = w->px; wp.y = w->py; wp.z = w->pz;
        if (!(p.flg & 0x4u) && recvx_t3_attack_hit_check(&wp, 5.0f)) { /* :177 */
            p.flg |= 0x10004u; p.stflg |= 0x10000u;                 /* :179-180 */
            p.mode0 = 2; p.mode1 = 0; p.mode2 = 0; p.mode3 = 0;     /* :182-185 */
            p.hp -= 20; if (p.hp < 0) p.hp = 0;                     /* :187-192 */
            put_player(w, &p);
        }
        w->yn -= 0.4f;                                              /* :195 */
        w->py += w->yn;                                             /* :197 */
        if (w->py < ep->py) {                                       /* :199 */
            w->py = ep->py;
            pos.x = w->px; pos.z = w->pz; pos.y = ep->py;           /* :203-205 */
            recvx_en02_set_sand_effect(ep, &pos, 7);                /* :207 */
            w->ct0 = 30;
            w->mode3++;
        }
        break;
    case 3:
        w->py -= 0.1f;                                              /* :216 */
        if (w->ct0-- == 0) { w->mdflg |= 0x1u; w->mode2 = 0; }      /* :218 */
        break;
    default: break;
    }
}
static void sub_mv04(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_WORK* ep = w->lkwkp; RECVX_ENEMY_VEC pos;
    if (!ep) { gap(w, "bhEne02sub_MV04: lkwkp NULL"); return; }
    switch (w->mode3) {
    case 0:
        w->flg |= 0x48u; w->mdflg &= ~0x1u;                         /* :236-237 */
        w->ct0 = 0;
        w->px = ep->px + (20.0f * njSin((Angle)ep->ay));            /* :241-242 */
        w->pz = ep->pz + (20.0f * njCos((Angle)ep->ay));
        w->ax = 0; w->ay = ep->ay;                                  /* :244-245 */
        w->yn = ep->py - 5.0f;                                      /* :247 */
        w->spd = 1.0f;                                              /* :249 */
        w->mode3++;
        /* kein break (:251) */
    case 1:
        w->ax += 2184;                                              /* :253 */
        SV(w, add_speed, "bhAddSpeed (en02sub MV04)");
        en02_sv->add_speed(w, 0);                                   /* :255 */
        w->py = w->yn + (2.0f * njSin((Angle)((w->ct0 * 2184) - 16384))); /* :257 */
        pos.x = w->px; pos.y = w->py; pos.z = w->pz;
        recvx_en02_set_sand_effect(ep, &pos, 8);                    /* :259 */
        if (w->ct0++ > 30) { w->mdflg |= 0x1u; w->flg &= ~0x40u; w->mode2 = 0; } /* :261 */
        break;
    default: break;
    }
}
void recvx_en02sub(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    switch (w->mode0) {                                             /* en02sub.c:26 bhEne02sub_Mode0[2] */
    case 0: sub_init(w); break;
    case 1:
        switch (w->mode2) {                                         /* :31 bhEne02sub_MoveMode2[5] */
        case 0: break;                                              /* MV00 leer */
        case 1: sub_mv01(w); break;
        case 2: sub_mv02(w); break;
        case 3: sub_mv03(w); break;
        case 4: sub_mv04(w); break;
        default: gap(w, "bhEne02sub_MoveMode2 index"); break;
        }
        break;
    default: gap(w, "bhEne02sub_Mode0 index"); break;
    }
}
