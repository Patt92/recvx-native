/* Identitaet 56 — nativ eigenstaendig bestimmt.
   Semantikreferenz: eneset.c:543 (bhEne56). Kein Originalkoerper, kein
   Generator, keine zweite Workwelt. Der geteilte UFO-Zustand (sys->ufo_*)
   und das Raumobjekt (sys->obwp) bleiben beim vorhandenen Besitzer und
   werden ausschliesslich ueber die Dienste gelesen/geschrieben. */
#include "en56.h"
#include <stddef.h>   /* NULL: set_shadow(jtb == NULL) laesst jno[0] = -1 */

/* Die vier Rumpfknoten, die der Original-Modus 2 gegen die Wand prueft, mit
   ihrem jeweiligen X-Versatz (eneset.c:645-686). Reine Datentabelle. */
static const struct { uint32_t joint; float dx; } en56_probe[4] = {
    { 3u, -0.6f }, { 5u, -0.6f }, { 7u, +0.6f }, { 9u, +0.6f }
};

/* Mode-0-Startlagen, abhaengig von Eventflag 0x3B (eneset.c:570-581). */
static void en56_start_position(RECVX_ENEMY_WORK* w, int flag_3b, RECVX_ENEMY_VEC* out)
{
    if (flag_3b) { out->x = 13.0f; out->y = -15.0f; out->z = 11.6f; }
    else         { out->x = 47.0f; out->y = -15.0f; out->z = 60.0f; }
    w->px = out->x; w->py = out->y; w->pz = out->z;
}

static void en56_mode1(RECVX_ENEMY_WORK* w, uint32_t oidx, int ufo_md, int ufo_flg);

void recvx_en56(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* sv;
    RECVX_ENEMY_VEC ufo_pos;
    RECVX_ENEMY_OBJECT op;
    int ufo_md = 0, ufo_flg = 0, flag_3b = 0;
    uint32_t oidx = 0;

    if (!w) return;
    sv = w->sv;

    /* Jeder Pass: nicht ueber bhSearchNearEnemy2 anvisierbar. */
    w->flg |= 0x8000u;

    if (!sv) return;
    if (!sv->ufo_get || !sv->ufo_get(&ufo_pos, &ufo_md, &ufo_flg, &oidx)) {
        if (sv->gap) sv->gap("bhEne56:sys->ufo_pos/ufo_md/ufo_flg");
        return;
    }
    flag_3b = sv->event_flag ? sv->event_flag(0x3Bu) : 0;
    if (!sv->event_flag && sv->gap) sv->gap("bhCkFlg(sys->ev_flg,0x3B)");

    switch (w->mode0) {
    case 0:
        w->mdflg |= 0x20u;

        /* mnwP = sys->rmthp: die Motion kommt aus der Raum-MTN-Liste, nicht
           aus der eingebauten Gegnerbibliothek. */
        w->mtn_room = 1;
        w->mtn_no   = 0;
        w->mtn_md   = 0;
        w->mtn_tp   = 0;

        w->hokan_rate  = 32768;
        w->hokan_count = 0;
        w->mtn_add     = 0;
        w->frm_no      = 0;

        /* sys->ufo_oidx ist im Original fest 4. */
        oidx = 4u;
        if (sv->ufo_set_oidx) sv->ufo_set_oidx(oidx);
        else if (sv->gap)     sv->gap("bhEne56:sys->ufo_oidx");

        en56_start_position(w, flag_3b, &ufo_pos);
        if (sv->ufo_set_pos) sv->ufo_set_pos(&ufo_pos);

        /* Schatten genau einmal (flg 0x800 ist die Marke); Original
           eneset.c:585 bhSetShadow(NULL, (unsigned char*)ep, 1, 12, 10, 12). */
        if (!(w->flg & 0x800u)) {
            if (sv->set_shadow) w->ct3 = sv->set_shadow(NULL, w, 1, 12.0f, 10.0f, 12.0f);
            else if (sv->gap)   sv->gap("bhSetShadow");
            w->flg |= 0x800u;
        }

        if (flag_3b) {
            /* Das UFO haengt sofort am Raumobjekt und bleibt in Mode 2. */
            if (sv->object_get && sv->object_get(oidx, &op)) {
                op.flg  |= 0x80u;
                op.lkono = 0;
                op.lox   = 0.0f;
                op.loy   = 42.0f;
                op.loz   = 0.0f;
                if (sv->object_set) sv->object_set(oidx, &op, w);
            } else if (sv->gap) {
                sv->gap("bhEne56:sys->obwp[ufo_oidx]");
            }
            w->mode0 = 2;
            break;
        }

        w->mode0 = 1;
        /* ORIGINALER DURCHFALL (eneset.c:608-609): Mode 0 laeuft im selben
           Pass direkt in Mode 1 weiter. Nicht in zwei Paesse trennen. */
        /* fall through */
    case 1:
        en56_mode1(w, oidx, ufo_md, ufo_flg);
        break;

    case 2:
        /* Festgehaltene Endpose: Frame 70 (0x460000 in 16.16), Vorschub 0. */
        w->frm_no = 0x460000;
        if (sv->set_motion) sv->set_motion(w, 0, w->mtn_md);
        else if (sv->gap)   sv->gap("bhSetMotion");
        if (sv->calc_model) sv->calc_model(w);
        else if (sv->gap)   sv->gap("bhCalcModel");
        break;

    default:
        break;
    }
}

static void en56_mode1(RECVX_ENEMY_WORK* w, uint32_t oidx, int ufo_md, int ufo_flg)
{
    const RECVX_ENEMY_SERVICES* sv = w->sv;
    RECVX_ENEMY_VEC ufo_pos, pos;
    RECVX_ENEMY_OBJECT op;
    float mtx[16];
    int i;

    /* Die Lage kommt in Mode 1 vom geteilten UFO-Zustand, nicht umgekehrt. */
    if (sv->ufo_get && sv->ufo_get(&ufo_pos, &ufo_md, &ufo_flg, &oidx)) {
        w->px = ufo_pos.x; w->py = ufo_pos.y; w->pz = ufo_pos.z;
    }

    if (sv->set_motion) sv->set_motion(w, w->mtn_add, w->mtn_md);
    else if (sv->gap)   sv->gap("bhSetMotion");
    if (sv->calc_model) sv->calc_model(w);
    else if (sv->gap)   sv->gap("bhCalcModel");

    switch (ufo_md) {
    case 0:
        if (sv->event_flag && sv->event_flag(0x3Bu)) {
            w->mode0 = 2;
        } else {
            w->mtn_add = 0;
            w->frm_no  = 0;
        }
        break;

    case 1:
        /* Vorschub nur bis Frame 40, danach angehalten. */
        w->mtn_add = ((w->frm_no / 65536) < 40) ? 65536 : 0;
        break;

    case 2:
        /* Vier Rumpfknoten gegen die Wand; der erste Treffer schaltet um. */
        for (i = 0; i < 4; ++i) {
            if (!(sv->joint_matrix && sv->joint_matrix(w, en56_probe[i].joint, mtx))) {
                if (sv->gap) sv->gap("bhEne56:mlwP->owP[n].mtx");
                break;
            }
            pos.x = mtx[12] + en56_probe[i].dx;
            pos.y = mtx[13] - 10.0f;
            pos.z = mtx[14];
            if (!sv->wall_type) { if (sv->gap) sv->gap("bhCheckWallType"); break; }
            if (sv->wall_type(&pos, 0u, 2.0f, 3.0f)) {
                if (sv->ufo_set_md) sv->ufo_set_md(3);
                else if (sv->gap)   sv->gap("bhEne56:sys->ufo_md");
                break;
            }
        }
        break;

    /* ufo_md 3 hat im Original keinen Zweig: der Pass tut dann nichts. */

    case 4:
        w->mtn_add = ((w->frm_no / 65536) < 70) ? 65536 : 0;
        /* Das Objekt darf dem Gegner nicht naeher als 9 Einheiten kommen. */
        if (ufo_flg != 0 && sv->object_get && sv->object_get(oidx, &op)
            && sv->joint_matrix && sv->joint_matrix(w, 7u, mtx)) {
            if ((op.px - mtx[12]) < 9.0f) {
                op.px = mtx[12] + 9.0f;
                if (sv->object_set) sv->object_set(oidx, &op, 0);
            }
        }
        break;

    case 5:
        if (ufo_flg != 0 && sv->object_get && sv->object_get(oidx, &op)) {
            if (!(op.flg & 0x80u)) {
                /* Ankoppeln: der Linkoffset ist die aktuelle Differenz. */
                op.flg  |= 0x80u;
                op.lkono = 0;
                op.lox   = 0.0f;
                op.loy   = op.py - w->py;
                op.loz   = op.pz - w->pz;
                if (sv->object_set) sv->object_set(oidx, &op, w);
                if (sv->object_get) sv->object_get(oidx, &op);
            }
            if (ufo_flg == 1 && w->py >= -35.0f) {
                /* Abkoppeln an der Originalhoehe: die Weltlage des
                   Wurzelknotens wird die eigene Objektlage. */
                op.flg  &= ~0x80u;
                op.px    = op.root.x;
                op.py    = op.root.y;
                op.pz    = op.root.z;
                op.mode1 = 0;
                op.yn    = 0.0f;
                if (sv->object_set) sv->object_set(oidx, &op, 0);
                if (sv->ufo_set_md) sv->ufo_set_md(6);
                else if (sv->gap)   sv->gap("bhEne56:sys->ufo_md");
            }
        } else if (ufo_flg != 0 && sv->gap) {
            sv->gap("bhEne56:sys->obwp[ufo_oidx]");
        }
        break;

    case 6:
        if (sv->object_get && sv->object_get(oidx, &op)) {
            switch (op.mode1) {
            case 0:
                /* Fall mit Beschleunigung bis -1.0 je Pass. */
                op.py += op.yn;
                if (op.yn > -1.0f) op.yn -= 0.2f;
                if (op.py <= 0.0f) {
                    op.yn    = 1.0f;
                    op.mode1 = 1;
                    if (sv->system_se) sv->system_se(0x80000211);
                    else if (sv->gap)  sv->gap("CallSystemSe(0,0x80000211)");
                }
                break;
            case 1:
                /* Ausklingendes Zittern: der Zufallswert kommt aus dem
                   gemeinsamen EE-libc-Strom, auf 0..1 normiert mit 2^31. */
                if (sv->random_next) {
                    const float r = (float)sv->random_next() / 2147483648.0f;
                    op.py = op.yn * r;
                } else {
                    if (sv->gap) sv->gap("rand");
                    op.py = 0.0f;
                }
                op.yn *= 0.8f;
                if (op.yn < 0.1f) {
                    op.mode1 = 2;
                    op.py    = 0.0f;
                    op.yn    = 0.0f;
                }
                break;
            case 2:
            default:
                break;
            }
            if (sv->object_set) sv->object_set(oidx, &op, 0);
        } else if (sv->gap) {
            sv->gap("bhEne56:sys->obwp[ufo_oidx]");
        }
        break;

    default:
        break;
    }
}
