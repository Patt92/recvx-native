/* Native EN14, Alexia: eigenständig auf der WorldEntity-Work implementiert.
   Semantikreferenz (nicht kompiliert/kopiert): src/ps2/veronica/prog/en14.c.
   Reihenfolge :181: Zustand -> SE -> Motion -> Tail -> Wand -> Flügel ->
   CalcModel -> Blick -> Waffenkapsel -> Spieler -> Suche Eff130/type4.
   EN13 besitzt/steuert den Parent-Link und verarbeitet Typ-0-Schaden direkt.
   Brain/NG00/Die/MV03/MV05..10 sind im Original tatsächlich leer.
   Zusätzliche Dienste in en14.h bleiben explizite Integrationsabhängigkeiten.
   Keine Generatoren, kein Originalkern, kein zweiter sys/plp/eff-Besitzer. */
#include "en14.h"
#include "ninja_matrix.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(RECVX_EN14_STATE) == 584, "EN14 EXP allocation");
_Static_assert(offsetof(RECVX_EN14_STATE, position) == 0x138, "EN14 tail");
_Static_assert(offsetof(RECVX_EN14_STATE, look_ticks) == 0x240, "EN14 look");

/* Ausschließlich reine Originaldaten, en14.c:18-142. */
static const uint8_t flip_tree[22] = {
    0,1,2,3,4,5,6,9,10,7,8,11,12,13,14,15,16,17,18,19,20,21
};
static const int8_t shadow_joints[5] = {1,6,11,12,-1};
static const int8_t clip_joints[8] = {6,1,7,9,11,15,18,21};
static const RECVX_ENEMY_CPCL capsules[25] = {
    {1,2,18},{2,3,18},{3,3,30},{0,25,0},{3,3,20},{15,40,0},
    {3,3,20},{-15,40,0},{7,7,12},{0,0,0},{8,8,12},{0,0,0},
    {9,9,12},{0,0,0},{10,10,12},{0,0,0},{3,4,12},{4,5,8},
    {5,6,6},{6,6,15},{0,15,0},{1,11,20},{1,1,30},{0,-20,0},{0,0,0}
};
typedef struct { int8_t type[3], bloodstain[3], exef; } DamageReaction;
static const DamageReaction reaction[21] = {
    {{0,1,0},{0,0,0},0}, {{0,1,0},{1,0,0},0}, {{0,1,0},{1,0,0},0},
    {{1,1,0},{1,0,0},0}, {{1,1,0},{1,0,0},0}, {{0,0,0},{1,0,0},0},
    {{1,1,0},{1,0,0},0}, {{0,0,0},{1,0,0},0}, {{0,0,0},{0,0,0},0},
    {{0,0,0},{0,0,0},0}, {{0,0,0},{1,0,0},0}, {{2,1,0},{1,0,0},0},
    {{0,0,0},{0,0,0},0}, {{1,1,0},{1,0,0},0}, {{2,1,0},{1,1,1},1},
    {{-1,-1,-1},{1,0,0},2}, {{-1,-1,-1},{1,0,0},1},
    {{-1,-1,-1},{1,0,0},0}, {{2,2,2},{1,1,1},0},
    {{2,2,2},{1,0,0},1}, {{2,2,2},{1,1,1},0}
};
typedef struct { int32_t crit, pt[3], timeout, bonus; } ComboWeapon;
static const ComboWeapon combo_weapon[21] = {
    {0,{0,0,0},0,0},{0,{0,0,0},0,0},{4,{1,0,0},120,800},
    {20,{6,4,2},60,0},{20,{6,4,2},60,0},{20,{6,4,2},60,0},
    {0,{0,0,0},60,0},{0,{0,0,0},60,0},{25,{5,3,1},30,0},
    {0,{0,0,0},0,0},{0,{0,0,0},60,0},{1,{1,1,1},60,0},
    {25,{5,4,2},30,0},{0,{0,0,0},60,0},{0,{0,0,0},0,0},
    {0,{0,0,0},0,0},{0,{0,0,0},0,0},{0,{0,0,0},0,0},
    {0,{0,0,0},0,0},{0,{0,0,0},0,0},{0,{0,0,0},0,0}
};
/* CombJointTbl[22] ist vollständig null (:69). Keine andere Gelenkkorrektur. */
typedef struct { int32_t flg; RECVX_ENEMY_VEC offset; float rx,ry,rz; } Blood;
static const Blood blood[22] = {
    {1,{0,0,0},0,0,0}, {0,{0,2,-2},5,3,1}, {0,{0,2,-2},5,3,1},
    {0,{0,1,-2},5,3,1}, {1,{0,0,-1},2,2,1}, {1,{0,0,-1},2,1,1},
    {1,{0,0,-1},2,1,1}, {1,{0,0,-1},1,1,1}, {1,{0,0,-1},1,1,1},
    {1,{0,0,-1},1,1,1}, {1,{0,0,-1},1,1,1},
    {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1},
    {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1},
    {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1},
    {0,{0,-1,0},1,2,1}, {0,{0,-1,0},1,2,1}
};
typedef struct { int32_t key; float count; } Shape;
static const Shape acid_front[5] = {{0,0},{5,1000},{12,1000},{20,0},{999,0}};
static const Shape acid_side[6] = {{0,0},{6,1000},{10,300},{16,1000},{25,0},{999,0}};

/* Nur Funktionszeiger, keine World-/Work-/Spielerdaten im Modul. */
static RECVX_EN14_RESOLVER service_resolver;
void recvx_en14_bind_services(RECVX_EN14_RESOLVER resolver) { service_resolver=resolver; }

typedef struct {
    RECVX_ENEMY_WORK* w;
    const RECVX_EN14_SERVICES* ex;
    RECVX_EN14_STATE* state;
} Pass;
static void gap(RECVX_ENEMY_WORK* w, const char* name)
{
    if (w && w->sv && w->sv->gap) w->sv->gap(name);
}
static int player(Pass* p, RECVX_ENEMY_PLAYER* out)
{
    if (p->w->sv && p->w->sv->player_get && p->w->sv->player_get(out)) return 1;
    gap(p->w,"bhEne14: Spielerbesitzer"); return 0;
}
static int joint(Pass* p, uint32_t node, NJS_MATRIX* out)
{
    if (p->w->sv && p->w->sv->joint_matrix && p->w->sv->joint_matrix(p->w,node,*out)) return 1;
    gap(p->w,"bhEne14: mlwP->owP Gelenkmatrix"); return 0;
}
static int random_ready(Pass* p)
{
    if (p->w->sv && p->w->sv->random_next) return 1;
    gap(p->w,"bhEne14: gemeinsamer rand"); return 0;
}
/* njRandom, macros.h:105: gemeinsamer rand-Strom, 31-Bit-Normierung. */
static float random01(Pass* p) { return (float)p->w->sv->random_next()/2147483648.0f; }
/* zonzon1.c:864 bhArcTan2: NICHT atan2f und NICHT njArcTan2! */
static int32_t arc(float y,float x)
{
    float radius=njSqrt(x*x+y*y);
    int32_t angle;
    if (radius==0.0f) return 0;
    angle=(int32_t)(10430.381f*asinf(y/radius));
    return x<0.0f ? (int16_t)(32768-angle) : angle;
}
static int32_t degrees(float deg) { return (int32_t)(deg*65536.0f/360.0f); }
static int32_t clamp_angle(int32_t a,int32_t lo,int32_t hi)
{
    return a<lo ? lo : (a>hi ? hi : a);
}
/* NJM_RAD_ANG wird in EN14 nicht verwendet; alle direkten asin-Konversionen
   nutzen ausdrücklich 10430.381f. Ungültige Ergebnisse melden statt UB beim
   float->int-Cast oder erfundenem Winkel. */
static int asin_angle(Pass* p,float v,int32_t* out)
{
    float a=10430.381f*asinf(v);
    if (!isfinite(a)) { gap(p->w,"bhEne14: undefinierter asin-Winkel"); return 0; }
    *out=(int32_t)a; return 1;
}
static void motion(RECVX_ENEMY_WORK* w,uint32_t no)
{
    w->mtn_no=no; w->frm_no=0; w->hokan_count=8; w->hokan_rate=45875;
}
static int clip_count(Pass* p,int32_t* count)
{
    uint32_t n=0;
    if (p->w->sv && p->w->sv->motion_frames) n=p->w->sv->motion_frames(p->w,p->w->mtn_no);
    if (!n) { gap(p->w,"bhEne14: mnwP[mtn_no].frm_num"); return 0; }
    *count=(int32_t)n-1; return 1;
}
static void se(Pass* p,int number)
{
    RECVX_ENEMY_VEC pos={p->w->px,p->w->py,p->w->pz};
    if (p->w->flg&0x10000u) return; /* zonzon1.c:828 */
    if (p->w->sv && p->w->sv->enemy_se && p->w->sv->enemy_now)
        p->w->sv->enemy_se(p->w->sv->enemy_now(),&pos,number);
    else gap(p->w,"bhEne_CallSE");
}
static void call_se(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    if (w->mtn_room) return;
    switch (w->mtn_no) {
    case 1: if (!w->frm_no) se(p,16851712); break;
    case 2: case 3: if (w->frm_no==458752) se(p,74499); break;
    case 4: if (!w->frm_no) se(p,8962); break;
    case 22: if (!w->frm_no) se(p,16786193); break;
    }
}

/* en14.c:1086. Vor Motion/CalcModel initialisierte vorhandene Matrizen,
   niemals eine neu berechnete Ersatzpose. */
static int tail_init(Pass* p)
{
    unsigned i;
    NJS_MATRIX m;
    if (!p->state) { gap(p->w,"bhEne14_TailInit: EXP0"); return 0; }
    for (i=0;i<11;++i) {
        if (!joint(p,11+i,&m)) return 0;
        p->state->position[i]=(RECVX_ENEMY_VEC){m[12],m[13],m[14]};
        p->state->velocity[i]=(RECVX_ENEMY_VEC){0,0,0};
        p->state->length[i]=2.5f;
    }
    return 1;
}
static void init(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    const RECVX_ENEMY_SERVICES* sv=w->sv;
    uint32_t i,now;
    w->flg=(w->flg|0x78u)&~6u; w->flg2|=1u;
    w->ar=8; w->ah=25; w->car=4; w->car=25; /* zwei car-Schreibzugriffe :235/236, ELF 1ddc7c/80 */
    w->hp=400;
    w->mode0=1; w->mode1=w->type==0 ? 0 : 1;
    w->mode2=w->type==0 ? 0 : 4; w->mode3=0;
    w->mtn_no=w->type==0 ? 0 : 5; w->mtn_md=0;
    w->hokan_rate=65536; w->hokan_count=0; w->mtn_add=65536;
    w->mtn_tp=flip_tree; w->frm_no=0;
    memcpy(w->clp_jno,clip_joints,sizeof clip_joints); w->mdflg&=~0x20u;
    if (!w->exp0) {
        if (!(sv && sv->calloc_work)) { gap(w,"bhEne_CallocWork EN14"); return; }
        w->exp0=sv->calloc_work(sizeof(RECVX_EN14_STATE));
    }
    p->state=(RECVX_EN14_STATE*)w->exp0;
    if (!p->state) { gap(w,"bhEne14: EXP0-Budget erschöpft"); return; }
    w->flg&=~0x80u;
    if (w->type==0) {
        if (!(sv && sv->enemy_now && sv->enemy_at)) gap(w,"bhEne14_Init: EN13-Workliste");
        else {
            now=sv->enemy_now();
            for (i=0;i<=now;++i) {
                RECVX_ENEMY_WORK* parent=sv->enemy_at(i);
                if (!parent) { gap(w,"bhEne14_Init: ene-Slot fehlt"); break; }
                if ((parent->flg&1u) && parent->id==13) {
                    w->lkwkp=parent; w->lkono=0;
                    w->px=parent->px; w->py=parent->py; w->pz=parent->pz; w->ay=parent->ay;
                    w->flg&=~0x10u; break;
                }
            }
        }
    }
    if (w->type==1) {
        if (!(w->flg&0x800u)) {
            if (sv && sv->set_shadow) {
                sv->set_shadow(shadow_joints,w,2,6,6,4); w->flg|=0x800u;
            } else gap(w,"bhSetShadow EN14");
        }
        w->stflg&=~8u;
    } else w->stflg|=8u;
    w->cpcl=capsules;
    tail_init(p);
}

/* en14.c:1403. Liest absichtlich frm_no NACH bhSetMotion. */
static void wing_motion(Pass* p)
{
    static const uint32_t mirror[4]={9,10,7,8};
    unsigned i;
    RECVX_ENEMY_WORK* w=p->w;
    if (!(p->ex && p->ex->motion_angles && p->ex->node_angles)) {
        gap(w,"bhEne14_SetMotion: Motion-/Knotenwinkel"); return;
    }
    for (i=0;i<4;++i) {
        int32_t angles[3];
        uint32_t source=(w->mtn_md&2u) ? mirror[i] : 7+i;
        if (!p->ex->motion_angles(w,w->mtn_no,source,(uint32_t)(w->frm_no/65536),angles)) {
            gap(w,"bhEne14_SetMotion: Winkelschlüssel fehlt"); return;
        }
        if (w->mtn_md&2u) { angles[1]=-angles[1]; angles[2]=-angles[2]; }
        p->ex->node_angles(w,7+i,angles);
    }
}

/* en14.c:1106. Trägheit, Rückstellung, verankerte Wurzel, zehn starre
   2.5-Segmente, Geschwindigkeitsdifferenz, inverse Kettenmatrix. */
static void tail_swing(Pass* p)
{
    unsigned i;
    NJS_MATRIX base,m,inv;
    RECVX_EN14_STATE* s=p->state;
    RECVX_ENEMY_WORK* w=p->w;
    if (!(s && w->sv && w->sv->calc_model && p->ex && p->ex->node_local && p->ex->node_angles)) {
        gap(w,"bhEne14_TailSwing: EXP/Poseanbieter"); return;
    }
    w->sv->calc_model(w);
    memcpy(s->previous,s->position,sizeof s->position);
    for (i=0;i<11;++i) {
        const float gain=i<2 ? 0.5f : 0.2f;
        RECVX_ENEMY_VEC* pos=&s->position[i];
        const RECVX_ENEMY_VEC* v=&s->velocity[i];
        if (!joint(p,11+i,&m)) return;
        pos->x+=v->x; pos->y+=v->y; pos->z+=v->z;
        pos->x+=(m[12]-pos->x)*gain;
        pos->y+=(m[13]-pos->y)*gain;
        pos->z+=(m[14]-pos->z)*gain;
        if (!i) memcpy(base,m,sizeof base);
    }
    s->position[0]=(RECVX_ENEMY_VEC){base[12],base[13],base[14]};
    for (i=0;i<10;++i) {
        NJS_VECTOR delta={s->position[i+1].x-s->position[i].x,
                          s->position[i+1].y-s->position[i].y,
                          s->position[i+1].z-s->position[i].z};
        njUnitVector(&delta);
        s->position[i+1]=(RECVX_ENEMY_VEC){s->position[i].x+2.5f*delta.x,
            s->position[i].y+2.5f*delta.y,s->position[i].z+2.5f*delta.z};
    }
    for (i=0;i<11;++i)
        s->velocity[i]=(RECVX_ENEMY_VEC){s->position[i].x-s->previous[i].x,
            s->position[i].y-s->previous[i].y,s->position[i].z-s->previous[i].z};
    if (!joint(p,1,&m)) return;
    for (i=0;i<10;++i) {
        int32_t angles[3],old[3];
        RECVX_ENEMY_VEC local;
        NJS_POINT3 target={s->position[i+1].x,s->position[i+1].y,s->position[i+1].z},v;
        if (!p->ex->node_local(w,11+i,&local,old)) { gap(w,"bhEne14_TailSwing: objP"); return; }
        memcpy(inv,m,sizeof inv);
        if (!njInvertMatrix(&inv)) { gap(w,"bhEne14_TailSwing: singuläre Kettenmatrix"); return; }
        njCalcPoint(&inv,&target,&v);
        angles[0]=clamp_angle(arc(-v.z,-v.y),degrees(-10),degrees(30));
        angles[1]=0;
        angles[2]=clamp_angle(arc(v.x,fabsf(v.y)),degrees(-10),degrees(10));
        p->ex->node_angles(w,11+i,angles);
        njTranslate(&m,local.x,local.y,local.z);
        njRotateXYZ(&m,angles[0],angles[1],angles[2]);
    }
}

/* en14.c:952: Blickmatrix auf Gelenk 6, nach CalcModel. Explizite lokale
   Matrizen statt globalem lcmat/Matrixstack. */
static void look_player(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    const RECVX_EN14_SERVICES* ex=p->ex;
    NJS_MATRIX desired,parent,current,relative,increment;
    NJS_VECTOR view,forward={0,0,-1},axis;
    int32_t angles[3],rx,ry,rz,target_roll,old_roll,turn;
    float cross;
    if (!(ex && ex->joint_flags)) { gap(w,"bhEne14_LookPlayaer: owP.flg"); return; }
    if (w->mtn_room) {
        w->flg&=~0x100000u; ex->joint_flags(w,6,2,0); return;
    }
    ex->joint_flags(w,6,2,2);
    if (!(p->state && ex->node_angles && ex->joint_matrix_set)) {
        gap(w,"bhEne14_LookPlayaer: EXP/Knoten-/Matrixschreiben"); return;
    }
    if (!joint(p,5,&parent)) return;
    if (!(w->flg&0x100000u)) {
        if (!(ex->motion_angles && ex->motion_angles(w,w->mtn_no,6,(uint32_t)(w->frm_no/65536),angles))) {
            gap(w,"bhEne14_LookPlayaer: Winkelschlüssel 6"); return;
        }
        if (w->mtn_md&2u) { angles[1]=-angles[1]; angles[2]=-angles[2]; }
        njUnitMatrix(&desired); njRotateXYZ(&desired,angles[0],angles[1],angles[2]);
    } else {
        NJS_MATRIX inv;
        NJS_POINT3 head;
        RECVX_ENEMY_VEC zero={0,0,0},point;
        if (!joint(p,6,&current)) return;
        if (!(w->sv && w->sv->player_joint_point && w->sv->player_joint_point(5,&zero,&point))) {
            gap(w,"bhEne14_LookPlayaer: Spieler-Gelenk5"); return;
        }
        head=(NJS_POINT3){point.x,point.y,point.z};
        view=(NJS_VECTOR){head.x-current[12],head.y-current[13],head.z-current[14]};
        njUnitVector(&view);
        memcpy(inv,parent,sizeof inv);
        if (!njInvertMatrix(&inv)) { gap(w,"bhEne14_LookPlayaer: singuläre Elternmatrix"); return; }
        njCalcVector(&inv,&view,&view);
        if (!asin_angle(p,view.y,&rx)) return;
        ry=arc(-view.x,-view.z);
        ry=clamp_angle(ry,degrees(-60),degrees(60));
        rx=clamp_angle(rx,degrees(-30),degrees(60));
        njUnitMatrix(&desired); njRotateXYZ(&desired,rx,ry,0);
        view=forward; njCalcVector(&desired,&view,&view);
        cross=njOuterProduct(&forward,&view,&axis);
        njUnitVector(&axis);
        if (!asin_angle(p,cross,&turn)) return;
        njUnitMatrix(&desired);
        /* Native njInvertSqrt(0) liefert FLT_MAX (ninja_math.c), wie der
           vorhandene Plattformanbieter. Auch Nullachse/Winkel0 läuft durch
           die originale Quaternionenform, kein Ersatz-Identity-Zweig. */
        njRotate(&desired,&axis,turn);
        if (!asin_angle(p,desired[1],&rz)) return;
        njRotateZ(&desired,-rz);
    }
    if (p->state->look_ticks) {
        if (!joint(p,6,&current)) return;
        memcpy(relative,parent,sizeof relative);
        if (!njInvertMatrix(&relative)) { gap(w,"bhEne14_LookPlayaer: singuläre Elternmatrix"); return; }
        njMultiMatrix(&relative,&current);
        if (!asin_angle(p,desired[1],&target_roll) || !asin_angle(p,relative[1],&old_roll)) return;
        NJS_VECTOR old_forward={relative[8],relative[9],relative[10]};
        NJS_VECTOR new_forward={desired[8],desired[9],desired[10]};
        cross=njOuterProduct(&old_forward,&new_forward,&axis);
        njUnitVector(&axis);
        if (!asin_angle(p,cross,&turn)) return;
        turn/=p->state->look_ticks;
        njUnitMatrix(&increment); njRotate(&increment,&axis,turn);
        njMultiMatrix(&increment,&relative); memcpy(desired,increment,sizeof desired);
        njRotateZ(&desired,(int16_t)(target_roll-old_roll)/p->state->look_ticks);
        --p->state->look_ticks;
    }
    rx=arc(desired[6],desired[5]); ry=arc(-desired[2],desired[0]);
    if (!asin_angle(p,desired[1],&rz)) return;
    angles[0]=rx; angles[1]=ry; angles[2]=rz;
    ex->node_angles(w,6,angles);
    njRotateXYZ(&parent,rx,ry,rz);
    ex->joint_matrix_set(w,6,parent);
}

/* en14.c:1311: Flugzeit aus y-Abstand, danach ein/vier einzelne Eff256.
   RNG immer Größe vor Allokation; drei weitere Werte NUR bei Erfolg. */
static void acid(Pass* p,int sound)
{
    RECVX_ENEMY_WORK* w=p->w;
    RECVX_ENEMY_PLAYER pl;
    NJS_MATRIX mouth,rotation;
    RECVX_ENEMY_VEC origin,velocity,pos;
    int i,count=w->type==0 ? 4 : 1;
    float t;
    if (!player(p,&pl) || !joint(p,6,&mouth) || !random_ready(p)) return;
    if (!(p->ex && p->ex->acid_alloc && p->ex->acid_setup)) { gap(w,"bhEne14_Acid: Eff256/SetEffectTb"); return; }
    origin=(RECVX_ENEMY_VEC){mouth[12],mouth[13],mouth[14]};
    velocity.y=njSqrt(0.0f);
    t=floorf((velocity.y+njSqrt(velocity.y*velocity.y-0.6f*(pl.pos.y-origin.y)))/0.3f);
    if (t<1) t=1;
    if (!isfinite(t)) { gap(w,"bhEne14_Acid: nichtendliche Flugzeit"); return; }
    velocity.x=(pl.pos.x-origin.x)/t; velocity.z=(pl.pos.z-origin.z)/t;
    for (i=0;i<count;++i) {
        float size=1.0f+0.5f*random01(p);
        int slot=p->ex->acid_alloc(w,w->type==0 ? 4 : 6,&origin,size);
        if (slot==-2) { gap(w,"bhEne14_Acid: Eff256-Allokator"); return; }
        if (slot!=-1) {
            NJS_VECTOR v={velocity.x,velocity.y,velocity.z};
            float advance;
            int32_t yaw=(int32_t)(1092.0f*random01(p)-546.0f);
            int32_t pitch=(int32_t)(910.0f*random01(p));
            njUnitMatrix(&rotation); njRotateY(&rotation,yaw); njRotateX(&rotation,pitch);
            njCalcVector(&rotation,&v,&v);
            advance=random01(p);
            pos=(RECVX_ENEMY_VEC){origin.x+advance*v.x,origin.y+advance*v.y,origin.z+advance*v.z};
            RECVX_ENEMY_VEC rotated={v.x,v.y,v.z};
            p->ex->acid_setup(w,slot,&pos,&rotated,sound);
        }
    }
}

/* zonzon1.c:698 ShapeCnt, mit den unveränderten fünf/sechs Datenpunkten. */
static int shape_count(Pass* p,const Shape* table,uint32_t n,int frame,float* out)
{
    uint32_t i;
    for (i=0;i<n;++i) if (frame<=table[i].key) {
        if (frame==table[i].key) { *out=table[i].count; return 1; }
        if (!i) break;
        *out=table[i-1].count+((table[i].count-table[i-1].count)*(frame-table[i-1].key))/(table[i].key-table[i-1].key);
        return 1;
    }
    gap(p->w,"bhEne14_MV02: ShapeTbl-Schlüssel außerhalb Tabelle"); return 0;
}
static void idle(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    if (!w->mode3) {
        w->mtn_md&=~2u; w->flg|=0x100000u;
        w->mtn_no=0; w->frm_no=0; w->hokan_count=18; w->hokan_rate=45875;
        ++w->mode3;
    }
}
static void retract(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    if (!w->mode3) {
        w->flg&=~0x100000u; p->state->look_ticks=16; motion(w,1);
        if (!clip_count(p,&w->ct0)) return;
        ++w->mode3;
    }
    if (w->mode3==1) {
        if (w->ct0--==0) { w->mode1=0; w->mode2=0; w->mode3=0; }
        if (w->frm_no==2949120) { w->flg|=0x100000u; p->state->look_ticks=16; }
    }
}
static void spit(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    if (!w->mode3) {
        RECVX_ENEMY_PLAYER pl;
        RECVX_ENEMY_WORK* parent=w->lkwkp;
        int32_t a;
        w->flg&=~0x100000u; p->state->look_ticks=8;
        if (!parent) { gap(w,"bhEne14_MV02: EN13-Verknüpfung"); return; }
        if (!player(p,&pl)) return;
        a=(int16_t)(arc(parent->px-pl.pos.x,parent->pz-pl.pos.z)-parent->ay);
        w->ct1=a<degrees(-30) ? 1 : (a>degrees(30) ? 2 : 0);
        motion(w,w->ct1 ? 3 : 2);
        if (w->ct1==2) w->mtn_md|=2u;
        if (!clip_count(p,&w->ct0)) return;
        if (!(p->ex && p->ex->morph_models)) { gap(w,"bhEne14_MV02: Morphmodelle0/1"); return; }
        p->ex->morph_models(w,0,1); w->mdflg|=2u; w->shp_ct=0;
        ++w->mode3;
    }
    if (w->mode3==1) {
        int frame=w->frm_no/65536;
        if (!shape_count(p,w->ct1 ? acid_side : acid_front,w->ct1 ? 6 : 5,frame,&w->shp_ct)) return;
        if (frame>=7 && frame<13) acid(p,1);
        if (frame==15) { w->flg|=0x100000u; p->state->look_ticks=4; }
        if (w->ct0--==0) { w->mode1=0; w->mode2=0; w->mode3=0; w->mdflg&=~2u; }
    }
}

/* njDistanceP2L (ps2_NaColi.c:77): unendliche Gerade, NICHT Segment.
   Keine neue Strahl-/Ausweichheuristik, kein Clamp eines negativen Radikanden. */
static float line_distance(const RECVX_ENEMY_VEC* point,const RECVX_ENEMY_VEC* base,const RECVX_ENEMY_VEC* direction)
{
    NJS_VECTOR d={direction->x,direction->y,direction->z};
    float norm=njInvertSqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    float x=point->x-base->x,y=point->y-base->y,z=point->z-base->z;
    float squared=x*x+y*y+z*z,projection;
    d.x*=norm; d.y*=norm; d.z*=norm;
    if (squared==0) return 0;
    projection=d.x*x+d.y*y+d.z*z;
    return njSqrt(squared-projection*projection);
}
static int read_beam(Pass* p,RECVX_ENEMY_VEC* pos,RECVX_ENEMY_VEC* direction)
{
    if (!(p->ex && p->ex->beam_line && p->ex->beam_line(p->state->beam,pos,direction))) {
        gap(p->w,"bhEne14: gespeicherte Eff130-Poolidentität"); return 0;
    }
    return 1;
}
static int arena(Pass* p,const RECVX_ENEMY_PLAYER* pl,RECVX_ENEMY_ATR* out)
{
    int hit;
    if (!(p->ex && p->ex->floor_enemy)) { gap(p->w,"bhCheckFloorEnemy EN14"); return -1; }
    hit=p->ex->floor_enemy(pl->flr_no,pl->pos.x,pl->pos.z,out);
    if (hit<0) gap(p->w,"bhCheckFloorEnemy EN14");
    return hit;
}
/* Zielwahl MV04/MV11. Alle Zufallsaufrufe werden getrennt sequenziert. */
static int choose_target(Pass* p,const RECVX_ENEMY_PLAYER* pl,int attacking)
{
    RECVX_ENEMY_WORK* w=p->w;
    RECVX_ENEMY_ATR area;
    NJS_MATRIX m;
    NJS_VECTOR center;
    int hit=arena(p,pl,&area),angle;
    float distance;
    if (hit<0) return 0;
    if (!hit || area.prm0!=14) return 1; /* Original behält xn/yn/zn */
    if (!random_ready(p)) return 0;
    center=(NJS_VECTOR){area.px+area.w/2.0f-pl->pos.x,0,area.pz+area.d/2.0f-pl->pos.z};
    njUnitMatrix(&m); njRotateY(&m,-w->ay); njCalcVector(&m,&center,&center);
    if (attacking) {
        angle=arc(w->pz-pl->pos.z,w->px-pl->pos.x);
        if (center.x<0) angle=(int32_t)(angle+(7281.0f+1820.0f*random01(p)));
        else angle=(int32_t)(angle-(7281.0f+1820.0f*random01(p)));
        distance=40.0f+20.0f*random01(p);
        w->xn=pl->pos.x+distance*njCos(angle); w->zn=pl->pos.z+distance*njSin(angle);
        w->yn=25.0f+pl->pos.y+30.0f*random01(p);
    } else if (random01(p)>0.5f) {
        angle=arc(w->pz-pl->pos.z,w->px-pl->pos.x);
        if (center.x<0) { angle=(int32_t)(angle+(5461.0f+3640.0f*random01(p))); w->mtn_md&=~2u; }
        else { angle=(int32_t)(angle-(5461.0f+3640.0f*random01(p))); w->mtn_md|=2u; }
        motion(w,13);
        distance=40.0f+20.0f*random01(p);
        w->xn=pl->pos.x+distance*njCos(angle); w->zn=pl->pos.z+distance*njSin(angle);
        w->yn=25.0f+pl->pos.y+30.0f*random01(p);
    } else {
        angle=arc(w->pz-pl->pos.z,w->px-pl->pos.x);
        if (w->py>55.0f+pl->pos.y) {
            angle=(int32_t)(angle+3640.0f*random01(p)); motion(w,15); w->mtn_md&=~2u;
            distance=50.0f+10.0f*random01(p);
            w->xn=pl->pos.x+distance*njCos(angle); w->zn=pl->pos.z+distance*njSin(angle);
            w->yn=25.0f+pl->pos.y+20.0f*random01(p);
        } else {
            angle=(int32_t)(angle-3640.0f*random01(p)); motion(w,12);
            distance=40.0f+20.0f*random01(p);
            w->xn=pl->pos.x+distance*njCos(angle); w->zn=pl->pos.z+distance*njSin(angle);
            w->yn=95.0f+pl->pos.y-20.0f*random01(p);
        }
    }
    return 1;
}
/* MV04/MV11 identischer Bewegungsmechanismus, andere Originalzustandsregeln. */
static void fly_toward_target(Pass* p,const RECVX_ENEMY_PLAYER* pl)
{
    RECVX_ENEMY_WORK* w=p->w;
    NJS_VECTOR delta={(w->xn-w->px)/16.0f,(w->yn-w->py)/16.0f,(w->zn-w->pz)/16.0f};
    float speed=njScalor(&delta);
    int32_t turn;
    if (speed>2.5f) { delta.x=2.5f*delta.x/speed; delta.y=2.5f*delta.y/speed; delta.z=2.5f*delta.z/speed; }
    w->px+=delta.x; w->py+=delta.y; w->pz+=delta.z;
    turn=(int16_t)(arc(w->px-pl->pos.x,w->pz-pl->pos.z)-w->ay);
    turn=clamp_angle(turn,-546,546);
    w->ay+=turn;
}
static void flying(Pass* p,int attacking)
{
    RECVX_ENEMY_WORK* w=p->w;
    RECVX_ENEMY_PLAYER pl;
    if (!w->mode3) {
        w->flg|=0x100000u; w->mtn_md&=~2u; p->state->look_ticks=8;
        w->flg&=~0x20000u; tail_init(p);
        motion(w,attacking ? 19 : 6); w->flg&=~0x80000u;
        w->ct0=attacking ? (w->hp>0 ? 1 : 3) : 4;
        if (attacking) se(p,74499);
        w->ct2=1; ++w->mode3;
    }
    if (w->mode3==1) {
        if (!player(p,&pl) || !choose_target(p,&pl,attacking)) return;
        w->ct1=attacking ? 30 : (w->hp>300 ? 35 : (w->hp>150 ? 45 : 50));
        ++w->mode3;
    } else if (w->mode3==2) {
        RECVX_ENEMY_VEC base,direction,goal,pos;
        if (!player(p,&pl)) return;
        fly_toward_target(p,&pl);
        if (attacking) {
            if (w->frm_no==327680) acid(p,0);
            if (w->frm_no>=393216 && w->frm_no<720897) acid(p,1);
        } else if (!w->frm_no) motion(w,6);
        if (w->ct1--==0) {
            if (attacking) {
                if (w->ct0--==0 || (pl.flg&4u)) { w->mode2=4; w->mode3=0; }
                else w->mode3=1;
            } else {
                --w->ct0;
                if (w->ct0<=0 && !(pl.flg&4u)) { w->mode2=11; w->mode3=0; }
                else w->mode3=1;
            }
        }
        if (!p->state->beam) return;
        if (!attacking && (!w->ct2 || w->hp<=300)) return;
        if (!read_beam(p,&base,&direction)) return;
        goal=(RECVX_ENEMY_VEC){w->xn,w->yn,w->zn}; pos=(RECVX_ENEMY_VEC){w->px,w->py,w->pz};
        if (attacking) {
            if (line_distance(&goal,&base,&direction)<20.0f) w->mode3=1;
            if (w->ct2 && line_distance(&pos,&base,&direction)<20.0f) { w->mode3=1; --w->ct2; }
        } else {
            if (line_distance(&goal,&base,&direction)<20.0f) { w->mode3=1; --w->ct2; }
            else if (line_distance(&pos,&base,&direction)<20.0f) { w->mode3=1; --w->ct2; }
        }
    }
}

/* EN14-Auswertung des gemeinsamen bhEne_CalcDamage (zonzon1.c:725).
   Tabellensemantik unverändert; EN14s CombJointTbl ist null, daher flg0 /
   correct0 für alle 22 Gelenke. dam[0] zählt ausdrücklich NICHT mit. */
static int calc_damage(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    const ComboWeapon* cw;
    uint32_t n,i;
    NJS_MATRIX m;
    NJS_VECTOR hit={w->dvx,w->dvy,w->dvz};
    if (w->wpnr_no<0 || w->wpnr_no>=21) { gap(w,"bhEne14_HitMark: Waffenindex"); return 0; }
    if (!(w->sv && w->sv->obj_num) || !(n=w->sv->obj_num(w))) {
        gap(w,"bhEne_CalcDamage: mlwP->obj_num"); return 0;
    }
    if (n>22) { gap(w,"bhEne14_HitMark: CombJointTbl-Knotenzahl"); return 0; }
    cw=&combo_weapon[w->wpnr_no];
    w->comb_flg&=~0xDu; w->total_dam=0;
    for (i=1;i<n;++i) if (w->dam[i]) {
        int32_t damage=w->dam[i];
        if (w->comb_wep==(uint32_t)w->wpnr_no && w->comb_timeout) w->comb_timeout=cw->timeout;
        else { w->comb_wep=(uint32_t)w->wpnr_no; w->comb_pnt=0; w->comb_timeout=cw->timeout; }
        if (w->comb_flg&0x10u) w->comb_pnt+=cw->pt[0];
        if (w->comb_flg&0x20u) w->comb_pnt+=cw->pt[1];
        if (w->comb_flg&0x40u) w->comb_pnt+=cw->pt[2];
        if (cw->crit && w->comb_pnt>=cw->crit) {
            w->comb_flg|=1u;
            if (!(w->comb_flg&2u)) w->comb_pnt=0;
            damage+=(w->dam[i]*cw->bonus)/100;
        }
        w->total_dam+=damage;
    }
    njUnitMatrix(&m); njRotateY(&m,-w->ay); njCalcVector(&m,&hit,&hit);
    w->comb_flg|=hit.z>=0 ? 4u : 8u;
    return 1;
}
static RECVX_ENEMY_VEC blood_offset(Pass* p,const Blood* b,int mirror)
{
    RECVX_ENEMY_VEC v=b->offset;
    if (mirror) v.z=-v.z;
    v.x+=b->rx*random01(p)-b->rx/2.0f;
    v.y+=b->ry*random01(p)-b->ry/2.0f;
    v.z+=b->rz*random01(p)-b->rz/2.0f;
    return v;
}
int recvx_en14_hitmark_with_services(RECVX_ENEMY_WORK* w,const RECVX_EN14_SERVICES* ex)
{
    Pass p;
    const RECVX_ENEMY_SERVICES* sv;
    const DamageReaction* r;
    const Blood* b;
    int range=0,game_mode,i;
    RECVX_ENEMY_VEC offset;
    if (!w) return -1;
    p=(Pass){w,ex,(RECVX_EN14_STATE*)w->exp0}; sv=w->sv;
    if (!calc_damage(&p)) return -1;
    if (w->djnt_no<0 || w->djnt_no>=22) { gap(w,"bhEne14_HitMark: BloodTbl-Gelenkindex"); return -1; }
    r=&reaction[w->wpnr_no]; b=&blood[w->djnt_no];
    if (w->comb_flg&0x10u) range=0;
    if (w->comb_flg&0x20u) range=1;
    if (w->comb_flg&0x40u) range=2;
    if (r->type[range]>=0) {
        if (!random_ready(&p)) return -1;
        /* Auch bei Welt-Trefferpunkt werden die drei Offset-RNGs verbraucht. */
        offset=blood_offset(&p,b,!(w->comb_flg&4u));
        if (sv && sv->blood_effect_burst) {
            int world=(w->wpnr_no==10 || w->wpnr_no>=13);
            RECVX_ENEMY_VEC impact={w->dpx,w->dpy,w->dpz};
            sv->blood_effect_burst(w,r->type[range],w->djnt_no,world ? &impact : &offset,world);
        } else gap(w,"bhEne_SetBloodEffectBurst EN14");
    }
    if ((r->exef&1) && !b->flg) {
        if (!random_ready(&p)) return -1;
        for (i=0;i<4;++i) {
            float size;
            int lifetime;
            offset=blood_offset(&p,b,0);
            /* ELF HitMark 1e0290/1e02c4: Größe zuerst, Dauer danach. */
            size=0.5f+0.5f*random01(&p);
            lifetime=20+(int32_t)(40.0f*random01(&p));
            if (sv && sv->fire_effect) sv->fire_effect(w,w->djnt_no,&offset,size,lifetime);
            else gap(w,"bhEne_SetFireEffect EN14");
        }
    }
    if (r->exef&2) {
        if (!random_ready(&p)) return -1;
        offset=blood_offset(&p,b,0);
        if (sv && sv->acid_effect) sv->acid_effect(w,w->djnt_no,&offset,2);
        else gap(w,"bhEne_SetAcidEffect EN14");
    }
    if (w->type==1) w->hp-=w->total_dam;
    if (!(ex && ex->game_mode && ex->game_mode(&game_mode))) { gap(w,"bhEne14_HitMark: sys->gm_mode"); return -1; }
    if (game_mode!=3) {
        if (w->hp<0) w->hp=0;
        if (w->wpnr_no==18) {
            RECVX_ENEMY_PLAYER pl;
            w->flg|=2u;
            if (player(&p,&pl)) {
                pl.mtn_enemy=0;
                if (sv && sv->player_set) sv->player_set(&pl);
                else gap(w,"bhEne14_HitMark: Spieler-Motionquelle zurück");
            }
        }
    } else if (w->hp<0) w->flg|=2u;
    return w->total_dam;
}
int recvx_en14_hitmark(RECVX_ENEMY_WORK* w)
{
    return recvx_en14_hitmark_with_services(w,service_resolver ? service_resolver(w) : NULL);
}
static void hurt(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    if (w->mode2>1) { gap(w,"bhEne14_DamageMode2"); return; }
    if (!w->mode3) {
        w->flg&=~0x100000u; p->state->look_ticks=8;
        motion(w,w->mode2 ? 22 : 4);
        if (!clip_count(p,&w->ct0)) return;
        w->mtn_md&=~2u; ++w->mode3;
    }
    if (w->mode3==1 && w->ct0--==0) {
        w->mode0=1; w->mode1=0; w->mode2=0; w->mode3=0;
        w->flg=(w->flg&~4u)|0x100000u; p->state->look_ticks=8;
    }
}
static void move(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    switch (w->mode2) {
    case 0: idle(p); break;
    case 1: retract(p); break;
    case 2: spit(p); break;
    case 4: flying(p,0); break;
    case 11: flying(p,1); break;
    case 3: case 5: case 6: case 7: case 8: case 9: case 10: break; /* Original leer */
    default: gap(w,"bhEne14_MoveMode2"); break;
    }
    if (w->mode1==1 && w->type>1) gap(w,"bhEne14_BrainType"); /* BR00/01 leer */
    if (w->type==1 && (w->flg&4u)) {
        w->flg&=~4u;
        recvx_en14_hitmark_with_services(w,p->ex);
    }
}
/* en14.c:1451: dreistufige Originalreaktion auf Eff256-Spielerschaden.
   Kein eigener player work. Sicht im selben Pass zurück zum Besitzer. */
static void control_player(Pass* p)
{
    RECVX_ENEMY_WORK* w=p->w;
    RECVX_ENEMY_PLAYER pl;
    int voice=0,vibrate=0,changed=0;
    if (!player(p,&pl)) return;
    if (pl.mode0!=4 || (w->flg&2u) || pl.mode2>1) return;
    if (!(w->sv && w->sv->player_set)) { gap(w,"bhEne14_PlayerControl: Spieler schreiben"); return; }
    switch (pl.mode3) {
    case 0:
        pl.flg=(pl.flg&~0x40000u)|0x10000u; pl.flg2|=1u;
        pl.mtn_no=pl.mode2==0 ? 71 : 72;
        pl.frm_no=0; pl.hokan_count=3; pl.hokan_rate=32768; pl.mtn_add=65536;
        ++pl.mode3; changed=1; voice=1; vibrate=1;
        break;
    case 1:
        if (!pl.frm_no) {
            pl.mtn_enemy=1; pl.mtn_enemy_owner=w->id;
            pl.mtn_no=pl.mode2==0 ? 31 : 32;
            pl.frm_no=0; pl.hokan_count=3; pl.hokan_rate=32768; pl.mtn_add=65536;
            ++pl.mode3; changed=1;
        }
        break;
    case 2:
        if (!pl.frm_no) {
            pl.mtn_enemy=0; pl.flg=(pl.flg&~0x10004u)|8u; pl.flg2&=~1u;
            pl.at_flg=0; pl.stflg&=~0x10000u;
            pl.mode0=1; pl.mode1=pl.mode2=pl.mode3=0; changed=1;
        }
        break;
    }
    if (changed) w->sv->player_set(&pl);
    if (voice) {
        if (w->sv->player_voice) w->sv->player_voice(1026);
        else gap(w,"bhEne_CallPlayerVoice EN14");
    }
    if (vibrate) {
        if (w->sv->vibration) w->sv->vibration(1,9);
        else gap(w,"StartVibrationEx EN14");
    }
}
void recvx_en14_with_services(RECVX_ENEMY_WORK* w,const RECVX_EN14_SERVICES* ex)
{
    Pass p;
    const RECVX_ENEMY_SERVICES* sv;
    if (!w) return;
    p=(Pass){w,ex,(RECVX_EN14_STATE*)w->exp0}; sv=w->sv;
    if (w->mode0!=0 && !p.state) { gap(w,"bhEne14: EXP0 fehlt"); return; }
    switch (w->mode0) {
    case 0: init(&p); break;
    case 1: move(&p); break;
    case 2: if (w->mode2) gap(w,"bhEne14_NageMode2"); break; /* NG00 leer */
    case 3: hurt(&p); break;
    case 4: break; /* bhEne14_Die leer, keine neue Todesanimation */
    case 5:
        if (sv && sv->event) sv->event(w);
        else gap(w,"bhEne_Event EN14");
        break;
    default: gap(w,"bhEne14_Mode0"); break;
    }
    if (!p.state) return;
    call_se(&p);
    if (sv && sv->set_motion) sv->set_motion(w,w->mtn_add,w->mtn_md);
    else gap(w,"bhSetMotion EN14");
    if (w->flg&0x20000u) tail_swing(&p);
    if (w->flg&0x10u) {
        if (sv && sv->collision_works) {
            sv->collision_works(w);
            w->py-=8; sv->collision_works(w); w->py+=8;
            w->py+=8; sv->collision_works(w); w->py-=8;
        } else gap(w,"bhEne03_Collision EN14 (drei Höhen)");
    }
    if (w->type==1) wing_motion(&p);
    if (sv && sv->calc_model) sv->calc_model(w);
    else gap(w,"bhCalcModel EN14");
    look_player(&p);
    if (sv && sv->weapon_atr) sv->weapon_atr(w,6,1,5);
    else gap(w,"bhEne_SetWeponAtr EN14");
    if (w->type==1) control_player(&p);
    p.state->beam=0;
    if (!(ex && ex->find_beam && ex->find_beam(&p.state->beam)))
        gap(w,"bhEne14: eff[512] erste Eff130/type4-Suche");
}
void recvx_en14(RECVX_ENEMY_WORK* w)
{
    recvx_en14_with_services(w,service_resolver ? service_resolver(w) : NULL);
}
