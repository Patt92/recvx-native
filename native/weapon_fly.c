/* Eigenstaendige native Fassung von weapon.c:1148 bhCheckFlyAtari und
   weapon.c:1258 bhSetBowDamage (Semantikreferenz, nie kompiliert). */
#include "weapon_fly.h"
#include <math.h>
#include <stddef.h>

static void fly_gap(const RECVX_FLY_SERVICES* s, const char* name) {
    if (s && s->gap) s->gap(s->user, name);
}
static float fly_scalor(float x, float y, float z) { return sqrtf((x * x) + (y * y) + (z * z)); }

/* (pp->flg & 0x1) && (pp->flg & 0x20) && !(pp->flg & 0x2) && !(pp->stflg & 0x1000000)
   && (!(pp->flg & 0x80) || !(lkwkp->stflg & 0x1000000)) — weapon.c:1174 und :1275
   dieselbe Zeile. lkwkp NULL bei flg 0x80 dereferenziert das Original ohne Pruefung;
   der Vertrag in enemy_work.h behandelt das als Erzeugerluecke. */
static int fly_hittable(const RECVX_ENEMY_WORK* e, const RECVX_FLY_SERVICES* s) {
    if (!(e->flg & 0x1) || !(e->flg & 0x20) || (e->flg & 0x2) || (e->stflg & 0x1000000)) return 0;
    if (!(e->flg & 0x80)) return 1;
    if (!e->lkwkp) { fly_gap(s, "bhCheckFlyAtari: flg 0x80 ohne lkwkp (weapon.c:1174)"); return 0; }
    return !(e->lkwkp->stflg & 0x1000000);
}

/* weapon.c:1218-1232 / :1300-1312: das Gelenk j >= 1 mit dem kleinsten Abstand zu p
   (knr == 0 als "noch keins" wie im Original, also gewinnt bei Gleichstand das erste). */
static int32_t fly_nearest_joint(const RECVX_ENEMY_WORK* e, float px, float py, float pz,
                                 const RECVX_FLY_SERVICES* s) {
    int32_t kno = 0;
    float knr = 0;
    uint32_t j, num;
    float mtx[16];
    if (!e->sv || !e->sv->obj_num || !e->sv->joint_matrix) {
        fly_gap(s, "pp->mlwP->owP[j] (weapon.c:1218) ohne Gelenkbesitzer");
        return 0;
    }
    num = e->sv->obj_num(e);
    for (j = 1; j < num; j++) {
        float ll;
        if (!e->sv->joint_matrix(e, j, mtx)) {
            fly_gap(s, "pp->mlwP->owP[j] (weapon.c:1218) ausserhalb des Modells");
            continue;
        }
        ll = fly_scalor(px - mtx[12], py - mtx[13], pz - mtx[14]);
        if ((knr == 0) || (knr > ll)) { knr = ll; kno = (int32_t)j; }
    }
    return kno;
}

/* EneDamXxx[pp->id][wpn_no]: das Original indexiert ohne Grenze; ausserhalb laege
   fremder Speicher, deshalb benannte Luecke und kein Schaden. */
static int fly_damage(const int16_t (*tab)[22], const RECVX_ENEMY_WORK* e, int32_t wpn, int16_t* out,
                      const RECVX_FLY_SERVICES* s) {
    if (!tab) { fly_gap(s, "EneDamNear/Mid/Far (weapon.c) ohne Tabelle"); return 0; }
    if (e->id >= 31 || wpn < 0 || wpn >= 22) {
        fly_gap(s, "EneDam*[pp->id][wpn_no] ausserhalb [31][22] (weapon.c:1236)");
        return 0;
    }
    *out = tab[e->id][wpn];
    return 1;
}

int32_t recvx_check_fly_atari(RECVX_ENEMY_TABLE* table, const RECVX_GA_WORK* gap, int32_t eidx,
                              const RECVX_FLY_SERVICES* s) {
    int32_t id = -1;
    uint32_t i;
    RECVX_CB_SPHERE spr;
    if (!table) { fly_gap(s, "bhCheckFlyAtari: sys->ene ohne Gegnertabelle"); return -1; }
    spr.c.x = gap->px; spr.c.y = gap->py; spr.c.z = gap->pz;   /* :1163-1166 */
    spr.r = gap->r;
    for (i = 0; i < table->ewk_n && i < RECVX_ENEMY_SLOTS; i++) {
        RECVX_ENEMY_WORK* pp = table->slot[i];
        RECVX_CB_CAPSULE watr, wal;
        RECVX_CB_VEC ps, at;
        int32_t kno;
        int16_t dam = 0;
        if (!pp) continue;
        /* :1170 Tor, eidx ausgenommen, Kugel gegen die Kapsel des Gegners. */
        if (!fly_hittable(pp, s) || (int32_t)i == eidx) continue;
        watr.c1.x = pp->watr.c1.x; watr.c1.y = pp->watr.c1.y; watr.c1.z = pp->watr.c1.z;
        watr.c2.x = pp->watr.c2.x; watr.c2.y = pp->watr.c2.y; watr.c2.z = pp->watr.c2.z;
        watr.r = pp->watr.r;
        if (!recvx_cb_check_sc(&spr, &watr)) continue;
        /* :1172 npDistanceP2C, :1174-1182 Sichtkapsel r 0.1 bis zur Kugelmitte. */
        at.x = gap->px; at.y = gap->py; at.z = gap->pz;
        recvx_cb_distance_p2c(&at, &watr, &ps);
        wal.c1 = ps;
        wal.c2 = at;
        wal.r = 0.1f;
        if (!s || !s->c2_wall_n) { fly_gap(s, "bhCheckC2WallN (pwksub.c:2172) ohne Wandsatz"); continue; }
        if (s->c2_wall_n(s->user, &wal, 0x400) != 0) continue;
        /* :1188-1210 Trefferfelder. */
        pp->flg |= 0x4;
        id = (int32_t)i;
        pp->dpx = gap->px; pp->dpy = gap->py; pp->dpz = gap->pz;
        pp->dvx = gap->vx; pp->dvy = gap->vy; pp->dvz = gap->vz;
        pp->dax = gap->ax; pp->day = gap->ay;
        pp->wpnr_no = gap->wpn_no;
        kno = fly_nearest_joint(pp, gap->px, gap->py, gap->pz, s);
        if (kno < 0 || (uint32_t)kno >= RECVX_ENEMY_DAM) { fly_gap(s, "pp->dam[kno] ausserhalb dam[64]"); continue; }
        pp->djnt_no = kno;
        if (gap->at_flg & 0x80) {                                     /* :1236 fern */
            if (fly_damage(s ? s->dam_far : NULL, pp, gap->wpn_no, &dam, s)) pp->dam[kno] += dam;
            pp->comb_flg |= 0x40;
        } else if (fly_scalor(gap->px - gap->gx, gap->py - gap->gy, gap->pz - gap->gz) < 15.0f) {
            if (fly_damage(s ? s->dam_near : NULL, pp, gap->wpn_no, &dam, s)) pp->dam[kno] += dam;
            pp->comb_flg |= 0x10;                                     /* :1248 nah */
        } else {
            if (fly_damage(s ? s->dam_mid : NULL, pp, gap->wpn_no, &dam, s)) pp->dam[kno] += dam;
            pp->comb_flg |= 0x20;                                     /* :1254 mittel */
        }
    }
    return id;
}

void recvx_set_bow_damage(RECVX_ENEMY_TABLE* table, const RECVX_FLY_SERVICES* s) {
    uint32_t i;
    if (!table) { fly_gap(s, "bhSetBowDamage: sys->ene ohne Gegnertabelle"); return; }
    for (i = 0; i < table->ewk_n && i < RECVX_ENEMY_SLOTS; i++) {
        RECVX_ENEMY_WORK* pp = table->slot[i];
        int16_t far17 = 0;
        float px, py, pz;
        int32_t kno;
        if (!pp || !fly_hittable(pp, s)) continue;
        /* :1275 EneDamFar[pp->id][17] != 0 als letzte Bedingung der Zeile. */
        if (!fly_damage(s ? s->dam_far : NULL, pp, 17, &far17, s) || far17 == 0) continue;
        px = 0.5f * (pp->watr.c1.x + pp->watr.c2.x);                  /* :1277-1279 */
        py = 0.5f * (pp->watr.c1.y + pp->watr.c2.y);
        pz = 0.5f * (pp->watr.c1.z + pp->watr.c2.z);
        pp->flg |= 0x4;
        pp->dpx = px; pp->dpy = py; pp->dpz = pz;
        pp->dvx = 0; pp->dvy = 1.0f; pp->dvz = 0;
        pp->dax = 0; pp->day = 0;
        pp->wpnr_no = 17;
        kno = fly_nearest_joint(pp, px, py, pz, s);
        if (kno < 0 || (uint32_t)kno >= RECVX_ENEMY_DAM) { fly_gap(s, "pp->dam[kno] ausserhalb dam[64]"); continue; }
        pp->djnt_no = kno;
        pp->dam[kno] += (int32_t)(0.5f * (float)pp->hp);              /* :1316 */
        pp->comb_flg |= 0x40;
    }
}
