/* EN18 — eigenstaendige native Organ-/Tentakelsteuerung auf dem einzigen
   WorldEntity-Work. Semantikreferenzen (nicht kompiliert/kopiert):
   src/ps2/veronica/prog/en18.c:15-335 Daten, :359 Dispatch, :373 Init,
   :628-942 Bewegung, :945/:951 leere Nage/Damage, :957 Tod, :973 HitMark;
   zonzon1.c:698 ShapeCnt, :725 CalcDamage; en13.c:100-170 Child-Erzeugung,
   :629/:744 HitMark-Verbrauch und :1033-1095 Angriffsauftrag.
   Keine zweite Workwelt, keine Generatoren, kein Originalkern.

   HeartBeat (en18.c:242) wird in KEINEM EN18-Zweig benutzt; kein erfundener
   Herzschlag. InitAction type0 -> MV01, alle anderen -> MV00. MV03/Nage/
   Damage sind original leer. Es gibt keinen Motionvorschub oder eigenen
   SFX-/Brain-/Spielerschadenspass. EN13 besitzt diese Mechanismen.
   Ungebundene Dienste stoppen den betroffenen Zweig und melden die echte
   Luecke; fehlende Asset-/RNG-Werte werden nicht synthetisiert. */
#include "en18.h"
#include "en03_lib.h"
#include "ninja_matrix.h"
#include <stddef.h>

/* Nur Schnittstellenbindung; keine sys/plp/ene-Tabelle oder Workkopie. */
static const RECVX_EN18_SERVICES* en18_services;
void recvx_en18_bind(const RECVX_EN18_SERVICES* services)
{
    en18_services = services;
}

static void en18_gap(const RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}

/* Reine Daten aus en18.c:15/40/65. Keine Funktionskoerper. */
typedef struct { int8_t type[3]; uint8_t stain[3]; uint8_t extra; } EN18_REACTION;
static const EN18_REACTION en18_reactions[21] = {
    {{0,1,0},{0,0,0},0}, {{0,1,0},{0,0,0},0}, {{0,0,0},{0,0,0},0},
    {{1,1,0},{0,0,0},0}, {{1,1,0},{0,0,0},0}, {{1,1,0},{0,0,0},0},
    {{0,0,0},{0,0,0},0}, {{1,1,0},{0,0,0},0}, {{0,0,0},{0,0,0},0},
    {{0,0,0},{0,0,0},0}, {{0,0,0},{0,0,0},0}, {{0,0,0},{0,0,0},0},
    {{0,0,0},{0,0,0},0}, {{1,1,0},{0,0,0},0}, {{0,0,0},{0,0,0},1},
    {{-1,-1,-1},{0,0,0},2}, {{-1,-1,-1},{0,0,0},1},
    {{-1,-1,-1},{0,0,0},0}, {{1,1,1},{0,0,0},0},
    {{0,0,0},{0,0,0},1}, {{1,1,1},{0,0,0},1}
};
static const RECVX_T3_COMBWEP en18_weapons[21] = {
    {0,{0,0,0},0,0}, {0,{0,0,0},0,0}, {4,{1,0,0},30,0},
    {10,{4,3,1},20,0}, {10,{4,3,1},20,0}, {10,{4,3,1},10,0},
    {0,{0,0,0},25,0}, {0,{0,0,0},25,0}, {25,{5,3,1},5,0},
    {0,{0,0,0},0,0}, {0,{0,0,0},10,0}, {0,{0,0,0},30,0},
    {25,{5,4,2},10,0}, {0,{0,0,0},60,0}, {0,{0,0,0},0,0},
    {0,{0,0,0},0,0}, {0,{0,0,0},0,0}, {0,{0,0,0},0,0},
    {0,{0,0,0},0,0}, {0,{0,0,0},0,0}, {0,{0,0,0},0,0}
};
static const RECVX_T3_COMBJOINT en18_joints[14] = {
    {0,0},{0,0},{0,0},{0,0},{0,0},
    {0,-60},{0,-60},{0,-60},{0,-60},{0,-60},
    {0,-60},{0,-60},{0,-60},{0,-60}
};

typedef struct { RECVX_ENEMY_VEC offset; float rx,ry,rz; } EN18_OFFSET;
static const EN18_OFFSET en18_eo00[] = { {{0,0,-8},0,0,0} };
static const EN18_OFFSET en18_eo04[] = {
    {{0,0,-9},5,2,1},{{0,4.5f,-4.5f},4,4,2},{{0,6,-6},4,1,2},
    {{-6.6f,0,-5.8f},1,3,1},{{-6,3.5f,-3.5f},2,4,2},
    {{6.6f,0,-5.8f},1,3,1},{{6,3.5f,-3.5f},2,4,2}
};
static const EN18_OFFSET en18_eo05[] = {
    {{-1.6f,6.3f,-5.9f},4,1,5},{{-0.4f,6.3f,-1.6f},4,1,5},
    {{-4,2.4f,3},4,3,5},{{-4.5f,0,-4.9f},2,4,5},{{3.4f,3,-6.1f},2,4,5}
};
static const EN18_OFFSET en18_eo06[] = {
    {{-1.3f,5,-3.9f},4,1,5},{{0,6.5f,0.4f},4,1,5},{{1.5f,7.1f,5},4,1,5},
    {{-3.6f,0,0.6f},2,4,4},{{-4.6f,0,4.6f},2,4,4},
    {{1.5f,0,-5.8f},2,4,4},{{4.2f,0,-1},2,4,4}
};
static const EN18_OFFSET en18_eo07[] = {
    {{2.6f,7.1f,7.2f},4,2,3},{{1,4.9f,3.2f},4,2,3},{{0,3.6f,0},3,1,3},
    {{-0.4f,1.1f,-3.7f},2,1,3},{{2.8f,2.1f,2.5f},1,2,1},{{0.9f,2.1f,4.9f},1,2,1}
};
static const EN18_OFFSET en18_eo08[] = {
    {{1.6f,6.3f,-5.9f},4,1,5},{{0.4f,6.3f,-1.6f},4,1,5},
    {{4,2.4f,3},4,3,5},{{4.5f,0,-4.9f},2,4,5},{{-3.4f,3,-6.1f},2,4,5}
};
static const EN18_OFFSET en18_eo09[] = {
    {{1.3f,5,-3.9f},4,1,5},{{-0.0f,6.5f,0.4f},4,1,5},{{-1.5f,7.1f,5},4,1,5},
    {{3.6f,0,0.6f},2,4,4},{{4.6f,0,4.6f},2,4,4},
    {{-1.5f,0,-5.8f},2,4,4},{{-4.2f,0,-1},2,4,4}
};
static const EN18_OFFSET en18_eo10[] = {
    {{-2.6f,7.1f,7.2f},4,2,3},{{-1,4.9f,3.2f},4,2,3},{{-0.0f,3.6f,0},3,1,3},
    {{0.4f,1.1f,-3.7f},2,1,3},{{-2.8f,2.1f,2.5f},1,2,1},{{-0.9f,2.1f,4.9f},1,2,1}
};
static const EN18_OFFSET en18_eo11[] = {
    {{-0.5f,-1,-0.5f},4,4,4},{{-1.5f,2,0},4,4,4},{{-0.5f,-1,5.5f},4,4,4},
    {{3,3,7},4,4,4},{{4,3.5f,1.5f},4,4,4}
};
static const EN18_OFFSET en18_eo12[] = {
    {{0,4,3.3f},3,1,4},{{0,5,7.5f},3,1,5},{{-3.3f,0,4.2f},2,2,5},
    {{-2.6f,2.8f,9.3f},2,2,5},{{-5,1.4f,4},2,2,2},
    {{3.3f,0,4.2f},2,2,5},{{2.6f,2.8f,9.3f},2,2,5},{{5,1.4f,4},2,2,2}
};
static const EN18_OFFSET en18_eo13[] = {
    {{0.5f,-1,-0.5f},4,4,4},{{1.5f,2,0},4,4,4},{{0.5f,-1,5.5f},4,4,4},
    {{-3,3,7},4,4,4},{{-4,3.5f,1.5f},4,4,4}
};
typedef struct { int flag; const EN18_OFFSET* offsets; int count; } EN18_BLOOD;
static const EN18_BLOOD en18_blood[14] = {
    {1,en18_eo00,1},{0,en18_eo04,7},{0,en18_eo00,1},{0,en18_eo00,1},
    {0,en18_eo04,7},{0,en18_eo05,5},{0,en18_eo06,7},{1,en18_eo07,6},
    {0,en18_eo08,5},{0,en18_eo09,7},{1,en18_eo10,6},
    {0,en18_eo11,5},{0,en18_eo12,8},{0,en18_eo13,5}
};
static const RECVX_ENEMY_CPCL en18_capsules[31] = {
    {0,4,80},{0,5,60},{0,8,60},{8,5,55},{5,6,55},{6,7,30},
    {8,9,55},{9,10,30},{11,11,65},{25,-20,25},{12,12,65},{0,-20,40},
    {13,13,65},{-25,-20,25},{10,10,20},{5,0,-40},{7,7,20},{-5,0,-40},
    {4,4,30},{40,90,0},{4,4,30},{70,50,0},{4,4,30},{90,0,0},
    {4,4,30},{-40,90,0},{4,4,30},{-70,50,0},{4,4,30},{-90,0,0},{0,0,0}
};

/* Daten bleiben in Tabellenreihenfolge: SpawnAttacker hat Schluessel 25
   NACH Schluessel 32. GetShapeCnt sucht den ERSTEN >= ct0; nicht sortieren
   oder diese belegte Nichtmonotonie "korrigieren" (en18.c:259-276). */
static const RECVX_T3_PKEY en18_spawn[15] = {
    {0,0},{5,600},{10,800},{20,1000},{25,1000},{30,1100},{32,1800},
    {25,2000},{43,2100},{50,2140},{56,2300},{63,2700},{70,2950},
    {75,3000},{999,3000}
};
static const RECVX_T3_PKEY en18_tenta[11] = {
    {0,0},{5,300},{10,500},{15,1000},{25,1000},{30,1100},
    {32,1800},{35,2000},{115,2000},{140,3000},{999,3000}
};
static const RECVX_T3_PKEY en18_tenta2[11] = {
    {0,0},{5,300},{10,500},{15,1000},{25,1000},{30,1100},
    {32,1800},{35,2000},{95,2000},{114,3000},{999,3000}
};
static const RECVX_T3_PKEY en18_tenta3[11] = {
    {0,0},{5,300},{10,500},{15,1000},{25,1000},{30,1100},
    {32,1800},{35,2000},{120,2000},{145,3000},{999,3000}
};
static const RECVX_T3_PKEY en18_tenta4[11] = {
    {0,0},{5,300},{10,500},{15,1000},{25,1000},{30,1100},
    {32,1800},{35,2000},{100,2000},{120,3000},{999,3000}
};

static RECVX_EN18_EXT* en18_ext(RECVX_ENEMY_WORK* w)
{
    if (!w->exp0) en18_gap(w,"bhEne18: exp0 fehlt");
    return (RECVX_EN18_EXT*)w->exp0;
}
static int en18_random(RECVX_ENEMY_WORK* w, float* out)
{
    if (!w->sv || !w->sv->random_next) {
        en18_gap(w,"rand"); return 0;
    }
    /* Original -rand()/-2.1474836e9f: Float32 kann 1.0 erreichen. Ein
       Tabellenindex == num wird unten gemeldet, NICHT modulo/clamped. */
    *out = (float)w->sv->random_next() / 2147483648.0f;
    return 1;
}

/* Lokale Matrix statt globalem Ninja-Stack; gleiche Y-Rotation und
   Vector- (w=0), dann Welttranslation. en18.c:469/703/792/880. */
static RECVX_ENEMY_VEC en18_parent_point(const RECVX_ENEMY_WORK* parent,
                                       RECVX_ENEMY_VEC local)
{
    NJS_MATRIX matrix;
    NJS_VECTOR v = {local.x,local.y,local.z};
    njUnitMatrix(&matrix);
    njRotateY(&matrix,parent->ay);
    njCalcVector(&matrix,&v,&v);
    local.x = parent->px + v.x;
    local.y = parent->py + v.y;
    local.z = parent->pz + v.z;
    return local;
}
static int en18_wall_read(RECVX_ENEMY_WORK* w, RECVX_ENEMY_ATR* out)
{
    RECVX_EN18_EXT* ext = en18_ext(w);
    if (!ext) return 0;
    if (ext->mobile_wall < 0 || !w->sv || !w->sv->mobile_wall_get ||
        !w->sv->mobile_wall_get(ext->mobile_wall,out)) {
        en18_gap(w,"bhEne18: mobile ATR_WORK lesen"); return 0;
    }
    return 1;
}
static int en18_wall_write(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_ATR* in)
{
    RECVX_EN18_EXT* ext = en18_ext(w);
    if (!ext) return 0;
    if (ext->mobile_wall < 0 || !w->sv || !w->sv->mobile_wall_set) {
        en18_gap(w,"bhEne18: mobile ATR_WORK schreiben"); return 0;
    }
    w->sv->mobile_wall_set(ext->mobile_wall,in);
    return 1;
}
static int en18_wall_off(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_ATR wall;
    if (!en18_wall_read(w,&wall)) return 0;
    wall.flg = 0;
    return en18_wall_write(w,&wall);
}
static int en18_restore_root(RECVX_ENEMY_WORK* w)
{
    RECVX_EN18_EXT* ext = en18_ext(w);
    if (!ext) return 0;
    if (!en18_services || !en18_services->select_root || !ext->roots[0] ||
        !en18_services->select_root(w,ext->roots[0])) {
        en18_gap(w,"bhEne18: mlwP->objP restaurieren"); return 0;
    }
    return 1;
}
static int en18_pair(RECVX_ENEMY_WORK* w, uint32_t a, uint32_t b)
{
    RECVX_EN18_EXT* ext = en18_ext(w);
    if (!ext) return 0;
    if (!en18_services || !en18_services->morph_pair ||
        !ext->roots[a] || !ext->roots[b] ||
        !en18_services->morph_pair(w,ext->roots[a],ext->roots[b])) {
        en18_gap(w,"bhEne18: obj_a/obj_b Morphbindung"); return 0;
    }
    return 1;
}

static void en18_init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN18_EXT* ext;
    RECVX_ENEMY_WORK* parent = w->lkwkp;
    RECVX_ENEMY_ATR wall;
    float rng;
    if (!parent) { en18_gap(w,"bhEne18_Init: EN13 lkwkp fehlt"); return; }
    if (!(parent->flg & 1u) || parent->id != 13u) { w->flg=0; return; }
    if (w->type >= 10u) { en18_gap(w,"bhEne18_Init: InitAction type >= 10"); return; }

    w->flg = (w->flg | 0x8000u) & ~6u;
    w->mdflg |= 0x20u;
    w->ar=5.0f; w->ah=1.0f; w->car=3.0f;
    w->mode0=1; w->mode1=0; w->mode2=(w->type==0); w->mode3=0;
    w->hokan_rate=65536; w->hokan_count=0;
    w->mtn_no=0; w->mtn_md=0; w->mtn_add=65536; w->frm_no=0;
    if (!en18_random(w,&rng)) return;
    w->ct1 = rng > 0.5f ? 1 : -1;

    if (!w->exp0) {
        if (!w->sv || !w->sv->calloc_work) {
            en18_gap(w,"bhEne_CallocWork"); return;
        }
        ext=(RECVX_EN18_EXT*)w->sv->calloc_work((uint32_t)sizeof(*ext));
        if (!ext) { en18_gap(w,"bhEne18_Init: EXP-Allokation erschoepft"); return; }
        w->exp0=ext;
        ext->mobile_wall=-1;
        if (!en18_services || !en18_services->model_root) {
            en18_gap(w,"bhEne18_Init: mlwP[0..2].objP"); return;
        }
        for (uint32_t i=0;i<3u;++i) {
            ext->roots[i]=en18_services->model_root(w,i);
            if (!ext->roots[i]) { en18_gap(w,"bhEne18_Init: Modellroot fehlt"); return; }
        }
        if (!w->sv->mobile_wall_alloc) {
            en18_gap(w,"bhEne18_Init: sys->mwal_n++"); return;
        }
        ext->mobile_wall=w->sv->mobile_wall_alloc();
        if (!en18_wall_read(w,&wall)) return;
        wall.flg=0; wall.type=3; wall.w=0; wall.h=0; wall.d=0; wall.attr=0;
        if (!en18_wall_write(w,&wall)) return;
    }
    if (w->type < 4u) return;

    w->flg |= 0x28u;
    w->mdflg |= 1u;
    if (w->type==4u) {
        w->watr.c1=(RECVX_ENEMY_VEC){parent->px,parent->py+18.0f,parent->pz};
        w->watr.c2=(RECVX_ENEMY_VEC){parent->px,parent->py+5.0f,parent->pz};
        w->watr.r=11.0f;
        w->cpcl=en18_capsules;
        return;
    }
    /* Reine Kapseldaten der Typzweige en18.c:464-617. */
    static const RECVX_ENEMY_VEC starts[5] = {
        {-5,10,-0.0f},{5,10,0},{0,5,-10},{-10,5,-5},{10,5,-5}
    };
    static const RECVX_ENEMY_VEC ends[5] = {
        {-15,2,-30},{15,2,-30},{0,0,-20},{-18,0,-9},{18,0,-9}
    };
    const uint32_t k=w->type-5u;
    w->watr.c1=en18_parent_point(parent,starts[k]);
    w->watr.c2=en18_parent_point(parent,ends[k]);
    w->watr.r=k<2u ? 4.0f : 6.0f;
    w->flg2 |= 0x10u;
}

static void en18_idle(RECVX_ENEMY_WORK* w)
{
    if (w->mode3 != 0u) return;
    if (!en18_restore_root(w)) return;
    w->shp_ct=0.0f;
    w->mdflg &= ~2u;
    ++w->mode3;
}
static void en18_pulse(RECVX_ENEMY_WORK* w)
{
    if (w->mode3==0u) {
        w->mdflg |= 2u; w->ct0=0; ++w->mode3;
    }
    if (w->mode3 != 1u) return;
    if (w->ct0<0 || w->ct0>999) {
        en18_gap(w,"bhEne18_MV01: ShapeCnt ausserhalb Tabelle"); return;
    }
    w->shp_ct=recvx_t3_get_shape_cnt(en18_spawn,15,w->ct0);
    if (w->shp_ct<1000.0f) {
        if (!en18_pair(w,0,1)) return;
    } else {
        if (!en18_pair(w,1,2)) return;
        w->shp_ct-=1000.0f;
    }
    if (++w->ct0>120) w->ct0=0;
}

/* Die gemeinsame Zerlegung der MV02/04/05-Mechanik bewahrt jeden
   Zweig-/Aufrufunterschied: MV02 liest NUR HatchNo, Tentakel lesen danach
   TentaNo; Auswahl und Zeiten stehen unten explizit. */
static void en18_attack(RECVX_ENEMY_WORK* w, uint8_t mode)
{
    RECVX_ENEMY_ATR wall;
    RECVX_ENEMY_WORK* parent=w->lkwkp;
    const RECVX_T3_PKEY* keys;
    int count, end;
    if (!parent) { en18_gap(w,"bhEne18: EN13 lkwkp fehlt"); return; }
    if (w->mode3==0u) {
        w->mdflg |= 2u; w->ct0=0;
        if (!en18_services || !en18_services->parent_hatch_no ||
            !en18_services->parent_hatch_no(parent,&w->ct1)) {
            en18_gap(w,"bhEne13_GetHatchNo"); return;
        }
        if (mode!=2u && (!en18_services->parent_tenta_no ||
            !en18_services->parent_tenta_no(parent,&w->ct2))) {
            en18_gap(w,"bhEne13_GetTentaNo"); return;
        }
        static const RECVX_ENEMY_VEC hatch[3]={{0,0,-20},{-18,0,-10},{18,0,-10}};
        if (w->ct1<0 || w->ct1>=3) {
            en18_gap(w,"bhEne18: HatchNo ausserhalb pos[3]"); return;
        }
        if (!en18_wall_read(w,&wall)) return;
        wall.flg=1;
        const RECVX_ENEMY_VEC p=en18_parent_point(parent,hatch[w->ct1]);
        wall.px=p.x; wall.py=p.y; wall.pz=p.z;
        if (!en18_wall_write(w,&wall)) return;
        ++w->mode3;
    }
    if (w->mode3!=1u) return;
    if (w->ct0<0 || w->ct0>999) {
        en18_gap(w,"bhEne18: ShapeCnt ausserhalb Tabelle"); return;
    }
    if (mode==2u) { keys=en18_spawn; count=15; end=75; }
    else if (mode==4u) { keys=w->type==1u?en18_tenta3:en18_tenta; count=11; end=145; }
    else { keys=w->type==1u?en18_tenta4:en18_tenta2; count=11; end=122; }
    w->shp_ct=recvx_t3_get_shape_cnt(keys,count,w->ct0);
    if (!en18_wall_read(w,&wall)) return;
    if (w->shp_ct<1000.0f) {
        if (!en18_pair(w,0,1)) return;
        wall.w=(10.0f*w->shp_ct)/1000.0f;
    } else if (w->shp_ct<2000.0f) {
        if (!en18_pair(w,1,2)) return;
        w->shp_ct-=1000.0f; wall.w=10.0f;
    } else {
        if (!en18_pair(w,0,2)) return;
        w->shp_ct=3000.0f-w->shp_ct;
        wall.w=(10.0f*w->shp_ct)/1000.0f;
    }
    if (!en18_wall_write(w,&wall)) return;
    ++w->ct0;
    if (w->ct0==30) {
        if (mode==2u) {
            if (en18_services && en18_services->parent_put_attacker)
                en18_services->parent_put_attacker(parent,w->ct1);
            else en18_gap(w,"bhEne13_PutAttacker");
        } else {
            if (en18_services && en18_services->parent_tentacle)
                en18_services->parent_tentacle(parent,w->ct2);
            else en18_gap(w,"bhEne13_Tentacle");
        }
    }
    if (w->ct0>end) {
        w->mdflg &= ~2u; w->shp_ct=0.0f; w->mode2=0; w->mode3=0;
        en18_wall_off(w);
    }
}
static void en18_die(RECVX_ENEMY_WORK* w)
{
    w->flg |= 2u;
    if (!en18_restore_root(w)) return;
    w->shp_ct=0.0f; w->mdflg &= ~2u;
    /* Original nullprueft den ATR-Zeiger, aber niemals exp0. */
    RECVX_EN18_EXT* ext=en18_ext(w);
    if (ext && ext->mobile_wall>=0) en18_wall_off(w);
}
void recvx_en18(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    switch (w->mode0) {
    case 0: en18_init(w); break;
    case 1:
        switch (w->mode2) {
        case 0: en18_idle(w); break;
        case 1: en18_pulse(w); break;
        case 2: case 4: case 5: en18_attack(w,w->mode2); break;
        case 3: break; /* Original MV03 leer. */
        default: en18_gap(w,"bhEne18_MoveMode2"); break;
        }
        break;
    case 2: case 3: break; /* Original Nage/Damage leer. */
    case 4: en18_die(w); break;
    case 5:
        if (w->sv && w->sv->event) w->sv->event(w);
        else en18_gap(w,"bhEne_Event");
        break;
    default: en18_gap(w,"bhEne18_Mode0"); break;
    }
    /* Parent-Inaktivierung NACH Mode0, VOR CalcModel, auch nach Init ohne
       aktiven Parent. Keine eigene Death-Transition oder Child-Loeschung. */
    if (!w->lkwkp) en18_gap(w,"bhEne18: lkwkp fehlt");
    else if (w->lkwkp->stflg & 0x01000000u) {
        w->stflg |= 0x01000000u;
        en18_wall_off(w);
    }
    if (w->sv && w->sv->calc_model) w->sv->calc_model(w);
    else en18_gap(w,"bhCalcModel");
}

/* HitMark benutzt vier Zufallsaufrufe pro Offset (Auswahl, X,Y,Z), auch
   bei Welt-Trefferversatz: den scheinbar ungenutzten Offset NICHT entfernen.
   Float32-RAND_MAX-Rundung kann Index == count ergeben; dann offene
   Original-Tabellengrenze melden, keine Modulo-/Rand-Heuristik. */
static int en18_scatter(RECVX_ENEMY_WORK* w, const EN18_BLOOD* b, RECVX_ENEMY_VEC* out)
{
    float u;
    int index;
    if (!en18_random(w,&u)) return 0;
    index=(int)((float)b->count*u);
    if (index<0 || index>=b->count) {
        en18_gap(w,"bhEne18_HitMark: rand-Auswahl ausserhalb ENE18_EO"); return 0;
    }
    const EN18_OFFSET* p=&b->offsets[index];
    *out=p->offset;
    if (!en18_random(w,&u)) return 0;
    out->x+=(p->rx*u)-(p->rx/2.0f);
    if (!en18_random(w,&u)) return 0;
    out->y+=(p->ry*u)-(p->ry/2.0f);
    if (!en18_random(w,&u)) return 0;
    out->z+=(p->rz*u)-(p->rz/2.0f);
    return 1;
}
int recvx_en18_hitmark(RECVX_ENEMY_WORK* w)
{
    uint32_t n;
    int range=0;
    RECVX_ENEMY_VEC offset;
    const EN18_REACTION* react;
    const EN18_BLOOD* blood;
    if (!w) return -1;
    if (w->wpnr_no<0 || w->wpnr_no>=21 || w->djnt_no<0 || w->djnt_no>=14) {
        en18_gap(w,"bhEne18_HitMark: Waffe/Gelenk ausserhalb Tabellen"); return -1;
    }
    if (!w->sv || !w->sv->obj_num) {
        en18_gap(w,"bhEne_CalcDamage: mlwP->obj_num"); return -1;
    }
    n=w->sv->obj_num(w);
    if (!n || n>14u) {
        en18_gap(w,"bhEne18_HitMark: mlwP->obj_num ausserhalb CombJointTbl[14]"); return -1;
    }
    recvx_t3_calc_damage(w,en18_weapons,en18_joints);
    blood=&en18_blood[w->djnt_no];
    react=&en18_reactions[w->wpnr_no];
    if (w->comb_flg & 0x10u) range=0;
    if (w->comb_flg & 0x20u) range=1;
    if (w->comb_flg & 0x40u) range=2;
    if (react->type[range]>=0) {
        if (!en18_scatter(w,blood,&offset)) return -1;
        const int world=(w->wpnr_no==10 || w->wpnr_no>=13);
        const RECVX_ENEMY_VEC hit={w->dpx,w->dpy,w->dpz};
        if (w->sv->blood_effect_burst)
            w->sv->blood_effect_burst(w,react->type[range],w->djnt_no,world?&hit:&offset,world);
        else en18_gap(w,"bhEne_SetBloodEffectBurst");
        /* Alle Datenzeilen enthalten 0; keine zusaetzlichen Flecken
           erfinden. Der Originalzweig ist dennoch vollstaendig gebunden. */
        if (react->stain[range]) {
            if (en18_services && en18_services->bloodstain)
                en18_services->bloodstain(w,0,w->djnt_no,&offset);
            else en18_gap(w,"bhEne_SetBloodstain");
        }
    }
    if ((react->extra & 1u) && blood->flag==0 && (w->flg2 & 4u)) {
        for (int i=0;i<4;++i) {
            float size_random, length_random;
            if (!en18_scatter(w,blood,&offset)) return -1;
            /* Original-ELF bhEne18_HitMark 0x1EFDF0: nach Offset Z
               rand @0x1F0214 fuer size, @0x1F0248 fuer len; erst dann
               bhEne_SetFireEffect @0x1F0294. C-Argumentauswertung darf
               diese beiden Zufallswerte nicht vertauschen. */
            if (!en18_random(w,&size_random) || !en18_random(w,&length_random)) return -1;
            const float size=0.5f+(0.5f*size_random);
            const int length=(int)(40.0f*length_random)+20;
            if (w->sv->fire_effect) w->sv->fire_effect(w,w->djnt_no,&offset,size,length);
            else en18_gap(w,"bhEne_SetFireEffect");
        }
    }
    if ((react->extra & 2u) && blood->flag==0) {
        if (!en18_scatter(w,blood,&offset)) return -1;
        if (w->sv->acid_effect) w->sv->acid_effect(w,w->djnt_no,&offset,2.0f);
        else en18_gap(w,"bhEne_SetAcidEffect");
    }
    return w->total_dam;
}
