/* EN12 / Alexia I. Eigenstaendige Implementierung auf WorldEntity-Work.
   Semantikreferenz en12.c:478-2204, zonzon1.c:433-515/668-806/828-884.
   Originaldaten liegen ausschliesslich in en12_data.inc. Keine Original-
   Funktionskoerper, kein Generator, kein sys/plp/eff-Spiegel.
   Dienstluecken brechen den betroffenen Mechanismus ab und werden benannt;
   sie sind KEINE erfolgreichen Null-/Standardrueckgaben. */
#include "en12.h"
#include "ninja_matrix.h"
#include <math.h>
#include <string.h>

typedef struct { int8_t type[3], bloodstain[3]; uint8_t exef; } EN12_REACT;
typedef struct { int32_t crit, pt[3], timeout, bonus; } EN12_COMBO;
typedef struct { int32_t flg; RECVX_ENEMY_VEC ofp; float rx, ry, rz; } EN12_BLOOD;
typedef struct { int32_t key; float cnt; } EN12_SHAPE;
#include "en12_data.inc"

static void missing(RECVX_ENEMY_WORK* w, const char* name)
{ if (w->sv && w->sv->gap) w->sv->gap(name); }
#define NEED(w,field,name) do { if (!(w)->sv || !(w)->sv->field) { missing((w),(name)); return; } } while (0)
#define EXTRA(w,field,name) do { if (!extension(w)->services || !extension(w)->services->field) { missing((w),(name)); return; } } while (0)
static RECVX_EN12_EXT* extension(RECVX_ENEMY_WORK* w) { return (RECVX_EN12_EXT*)w->exp0; }
static const RECVX_EN12_SERVICES* extra(RECVX_ENEMY_WORK* w) { return extension(w)->services; }
static RECVX_ENEMY_VEC position(const RECVX_ENEMY_WORK* w)
{ RECVX_ENEMY_VEC p = {w->px,w->py,w->pz}; return p; }
static void put_position(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC p)
{ w->px=p.x; w->py=p.y; w->pz=p.z; }
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p)) {
        missing(w,"bhEne12: plp owner"); return 0;
    }
    return 1;
}
static int32_t arc(float x, float z)
{
    float len=njSqrt(x*x+z*z);
    int32_t a;
    if (len==0.0f) return 0;
    a=(int32_t)(10430.381f*asinf(x/len));
    return z<0.0f ? (int16_t)(32768-a) : a;
}
static int direction(RECVX_ENEMY_WORK* w, float x, float z)
{ return (int16_t)(arc(w->px-x,w->pz-z)-w->ay); }
static int in_cone(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, int limit)
{ int a=direction(w,p->pos.x,p->pos.z); return a<=limit && a>=-limit; }
static float random_unit(RECVX_ENEMY_WORK* w)
{ return (float)(-w->sv->random_next()) / -2147483648.0f; }
static RECVX_ENEMY_VEC rotate(RECVX_ENEMY_VEC v, int angle)
{
    NJS_MATRIX m; NJS_VECTOR a={v.x,v.y,v.z}, b;
    njUnitMatrix(&m); njRotateY(&m,angle); njCalcVector(&m,&a,&b);
    v.x=b.x; v.y=b.y; v.z=b.z; return v;
}
static void mode(RECVX_ENEMY_WORK* w, int a, int b, int c, int d)
{ w->mode0=(uint8_t)a; w->mode1=(uint8_t)b; w->mode2=(uint8_t)c; w->mode3=(uint8_t)d; }
static int frames(RECVX_ENEMY_WORK* w, uint32_t m, int32_t* n)
{
    uint32_t f;
    if (!w->sv || !w->sv->motion_frames || !(f=w->sv->motion_frames(w,m))) {
        missing(w,"bhEne12: MN_WORK.frm_num"); return 0;
    }
    *n=(int32_t)f; return 1;
}
/* GetPartsPos reproduziert die Kette aus lokalen objP-Knoten (nicht owP
   des letzten Zeichnens); njRotateEx(lv=0) bedeutet Z/Y/X. */
static int parts(RECVX_ENEMY_WORK* w, const int8_t* path, RECVX_ENEMY_VEC* p)
{
    NJS_MATRIX m; NJS_POINT3 q;
    if (!extra(w) || !extra(w)->local_node) { missing(w,"bhEne_GetPartsPos: objP"); return 0; }
    njUnitMatrix(&m); njTranslate(&m,w->px,w->py,w->pz);
    njRotateZ(&m,w->az); njRotateY(&m,w->ay); njRotateX(&m,w->ax);
    for (; *path!=-1; ++path) {
        RECVX_ENEMY_VEC pos; int32_t ang[3];
        if (!extra(w)->local_node(w,(uint8_t)*path,&pos,ang)) { missing(w,"bhEne_GetPartsPos: node"); return 0; }
        njTranslate(&m,pos.x,pos.y,pos.z);
        njRotateZ(&m,ang[2]); njRotateY(&m,ang[1]); njRotateX(&m,ang[0]);
    }
    njGetTranslation(&m,&q); p->x=q.x; p->y=q.y; p->z=q.z; return 1;
}
static int joint(RECVX_ENEMY_WORK* w, uint32_t node, RECVX_ENEMY_VEC* p)
{
    float m[16];
    if (!w->sv || !w->sv->joint_matrix || !w->sv->joint_matrix(w,node,m)) {
        missing(w,"bhEne12: owP joint"); return 0;
    }
    p->x=m[12]; p->y=m[13]; p->z=m[14]; return 1;
}
int recvx_en12_bind(RECVX_ENEMY_WORK* w, const RECVX_EN12_SERVICES* services)
{
    RECVX_EN12_EXT* e; int i;
    if (!w) return 0;
    if (!w->exp0) {
        if (!w->sv || !w->sv->calloc_work) { missing(w,"bhEne_CallocWork(en12)"); return 0; }
        e=(RECVX_EN12_EXT*)w->sv->calloc_work((uint32_t)sizeof(*e));
        if (!e) { missing(w,"bhEne_CallocWork(en12): exhausted"); return 0; }
        memset(e,0,sizeof(*e)); e->surface_slot=-1;
        for (i=0;i<16;++i) e->hand_slot[i]=-1;
        w->exp0=e;
    }
    extension(w)->services=services; return 1;
}

/* Keine neuen Child-Enemy-Works: en12 erzeugt nur Effekte im geteilten Pool. */
static void fire_hand(RECVX_ENEMY_WORK* w, int action);
static void hit_mark(RECVX_ENEMY_WORK* w);
static void control_player(RECVX_ENEMY_WORK* w);
static void init_damage(RECVX_ENEMY_WORK* w);

static void init(RECVX_ENEMY_WORK* w)
{
    RECVX_EN12_EXT* e=extension(w);
    RECVX_EN12_EFFECT_TEMPLATE t={0};
    static const int8_t clip[8]={5,1,7,11,13,13,17,21};
    w->flg=(w->flg|0x78u)&~6u;
    w->ar=3.5f; w->ah=1.0f; w->car=3.0f; w->hp=300;
    mode(w,1,1,1,0); w->mtn_no=0; w->mtn_md=0x20;
    w->hokan_rate=0; w->hokan_count=0; w->mtn_add=65536;
    w->mtn_tp=flip_tree; w->frm_no=0; memcpy(w->clp_jno,clip,sizeof(clip));
    w->mdflg &= ~0x20u;
    if (!e->original_allocated) {
        EXTRA(w,effect_tb,"bhSetEffectTb(en12 Init)");
        t.id=262; t.flg=1;
        e->surface_slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG,0,0,0);
        /* en12.c:558-559 speichert bei eno == -1 ungeprueft &eff[-1]. Das ist
           KEIN NULL-Zeiger; die Marke -2 haelt diesen undefinierten Zustand
           fest, damit FlameLiquid/Blood ihn nicht als exp1 = NULL ausgeben. */
        if (e->surface_slot<0) { e->surface_slot=-2; missing(w,"bhEne12_Init: eff[-1] original out-of-bounds"); }
        e->original_allocated=1;
    }
    e->wait[0]=30; e->wait[1]=120; e->wait[2]=0; e->wait[3]=0; e->wait[4]=60;
    if (!(w->flg&0x800u)) {
        NEED(w,set_shadow,"bhSetShadow(en12)");
        w->sv->set_shadow(SdwTab,w,2,6.0f,6.0f,4.0f); w->flg|=0x800u;
    }
    w->stflg&=~8u; w->lok_jno=5; w->cpcl=CapColTab;
    w->flg&=~0x100000u; e->look_count=16;
    EXTRA(w,system_flags_clear,"bhEne12_Init: rm_flg clear");
    extra(w)->system_flags_clear(1,1);
}

static int avoid(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p, int* out)
{
    RECVX_ENEMY_VEC a=position(w), b=p->pos, n;
    int i;
    if (!w->sv || !w->sv->line || !w->sv->wall_type) { missing(w,"bhEne12_AvoidWall"); return 0; }
    if (w->sv->line(1,&a,&b,&n,0,-1,0,0)) { *out=1; return 1; }
    b.x=(b.x-a.x)/10.0f; b.z=(b.z-a.z)/10.0f;
    for (i=0;i<10;++i) {
        a.x+=b.x; a.z+=b.z;
        if (w->sv->wall_type(&a,0,w->ar,1.0f)) { *out=1; return 1; }
    }
    *out=0; return 1;
}
static void pursue(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_PLAYER* p)
{
    int blocked, step, a;
    RECVX_ENEMY_VEC from=position(w), to;
    w->xn=p->pos.x; w->zn=p->pos.z;
    if (!avoid(w,p,&blocked)) return;
    if (blocked) {
        EXTRA(w,route_points,"bhCheckRoute(en12)");
        to.x=w->xn; to.y=w->yn; to.z=w->zn;
        extra(w)->route_points(&from,&p->pos,&to);
        w->xn=to.x; w->yn=to.y; w->zn=to.z;
    }
    step=in_cone(w,p,10922)?273:(in_cone(w,p,21845)?455:637);
    a=direction(w,w->xn,w->zn); w->ayp=a>step?step:(a< -step?-step:a);
    w->ay+=w->ayp;
}
static void brain(RECVX_ENEMY_WORK* w)
{
    static const uint8_t acts[2][16]={{1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3},
        {2,2,2,2,2,2,3,3,3,3,3,3,3,3,0,0}};
    static const int delays[4]={0,60,10,30};
    RECVX_ENEMY_PLAYER p; RECVX_ENEMY_VEC q; float dx,dy,dz,dist; int i,act;
    if (!player(w,&p)) return;
    dx=w->px-p.pos.x; dy=w->py-p.pos.y; dz=w->pz-p.pos.z;
    dist=njSqrt(dx*dx+dy*dy+dz*dz);
    for (i=0;i<5;++i) if (extension(w)->wait[i]) --extension(w)->wait[i];
    if ((p.stflg&0x80000000u) || w->mtn_no==15) return;
    if (dist<7.0f && in_cone(w,&p,21845) && p.hp>=0) {
        NEED(w,player_set,"bhEne12_BR00: player flags");
        mode(w,2,0,0,0); p.flg|=0x10004u; p.stflg|=0x10000u;
        w->sv->player_set(&p); return;
    }
    if ((w->mtn_no==1 || w->mtn_no-2u<2u) && w->frm_no!=0x3b0000) return;
    if (p.hp<0) { mode(w,1,0,6,0); return; }
    q.x=w->px-10.0f*njSin(w->ay); q.y=w->py; q.z=w->pz-10.0f*njCos(w->ay);
    NEED(w,wall_type,"bhCheckWallType(en12 Brain)");
    if (w->sv->wall_type(&q,0,3.0f,1.0f) || extension(w)->wait[0] ||
        !(dist<80.0f) || !in_cone(w,&p,21845)) return;
    /* en12.c:669/671 indiziert act[?][(int)(16.0f*ratio)] mit ratio<1; nur am
       Rand (rand nahe 0x7fffffff) rundet das float-Produkt auf genau 16.0f und
       der Index 16 liese act[?][16], einen Wert UNHALB des 16-Felder-Feldes
       (Stack-Muell G). Das Original kehrt auf diesem Pfad NICHT zurueck: es
       prueft mode[G] (en12.c:674) und exp0[G+1] (en12.c:678), beides ebenfalls
       Out-of-bounds, und erhoehnt EXP0_I(4) um +30 (en12.c:679) oder +5
       (en12.c:688). Da G undefiniert ist, waere jeder konkrete Ersatz eine
       Annaeherung; hier wird der Zweig angehalten und die Luecke benannt. Die
       Timer wurden bereits wie en12.c:608-612 erniedrigt (N:180); der Rueckgabe
       -Zweig entspricht einem der drei Originalausgaenge (kein Aufschlag). */
    i=(int)(16.0f*random_unit(w));
    if (i>=16) { missing(w,"bhEne12_BR00: rand table index 16"); return; }
    act=acts[in_cone(w,&p,0x2000)?0:1][i];
    if (act==1 && dist<15.0f) return;
    if (!extension(w)->wait[act]) {
        extension(w)->wait[0]+=30;
        if (act) { extension(w)->wait[act]+=delays[act]; w->mode1=0; w->mode2=(uint8_t)(act+1); w->mode3=0; }
    } else extension(w)->wait[0]+=5;
}
static void flame_liquid(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC to, int time)
{
    RECVX_ENEMY_VEC from,n; RECVX_EN12_EFFECT_TEMPLATE t={0}; RECVX_EN12_EFFECT_VIEW v;
    float len,scale; int slot;
    if (extension(w)->surface_slot==-2) { missing(w,"bhEne12_FlameLiquid: exp1 = &eff[-1] (en12.c:1815)"); return; }
    NEED(w,line,"bhCollisionCheckLine2(en12 FlameLiquid)");
    EXTRA(w,effect_tb,"bhSetEffectTb(en12 FlameLiquid)");
    EXTRA(w,effect_get,"eff owner(en12 FlameLiquid)"); EXTRA(w,effect_set,"eff owner(en12 FlameLiquid)");
    if (!joint(w,9,&from)) return;
    if (!w->sv->line(2,&from,&to,&n,17408,-1,0,0)) return;
    len=njSqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    if (len!=0) { n.x/=len; n.y/=len; n.z/=len; }
    to.x+=0.2f*n.x; to.y+=0.2f*n.y; to.z+=0.2f*n.z;
    scale=0.2f+0.8f*random_unit(w);
    t.id=265; t.flg=1; t.type=1; t.pos=to; t.scale.x=scale; t.scale.y=1; t.scale.z=scale;
    slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG|RECVX_EN12_EF_TYPE|RECVX_EN12_EF_POS|RECVX_EN12_EF_SX|RECVX_EN12_EF_SY|RECVX_EN12_EF_SZ,0,0,0);
    if (slot<0) return;
    if (!extra(w)->effect_get(slot,&v)) { missing(w,"eff slot(en12 FlameLiquid)"); return; }
    v.normal=n; v.exp1_slot=extension(w)->surface_slot;
    v.ax=(int)(10430.381f*acosf(n.y)); v.ay=arc(n.x,n.z); v.ct0=time;
    extra(w)->effect_set(slot,&v);
}
static void acid(RECVX_ENEMY_WORK* w)
{
    RECVX_EN12_EFFECT_TEMPLATE t={0}; RECVX_EN12_EFFECT_VIEW v; int i,slot;
    EXTRA(w,effect_tb,"bhSetEffectTb(en12 Acid)"); EXTRA(w,effect_get,"eff owner(en12 Acid)");
    EXTRA(w,effect_set,"eff owner(en12 Acid)"); EXTRA(w,effect_texture,"mdl[2].texP(en12 Acid)");
    if (!joint(w,5,&t.pos)) return;
    t.id=256; t.type=3; t.flg=1;
    for (i=0;i<4;++i) {
        NJS_MATRIX m; NJS_VECTOR src,dst; float u;
        t.scale.x=t.scale.y=t.scale.z=1.0f+random_unit(w);
        slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG|RECVX_EN12_EF_TYPE|RECVX_EN12_EF_POS|RECVX_EN12_EF_SX|RECVX_EN12_EF_SY|RECVX_EN12_EF_SZ,0,0,0);
        if (slot<0) continue; /* RNG danach NUR bei erfolgreicher Allokation. */
        if (!extra(w)->effect_get(slot,&v)) { missing(w,"eff slot(en12 Acid)"); return; }
        v.stflg|=0x20u; v.tex_id=0; v.ofy=0.4f;
        src.x=-3.5f*njSin(w->ay); src.y=0.15f; src.z=-3.5f*njCos(w->ay);
        njUnitMatrix(&m); njRotateX(&m,(int)(910.0f*random_unit(w))); njCalcVector(&m,&src,&dst);
        v.velocity.x=dst.x; v.velocity.y=dst.y; v.velocity.z=dst.z;
        u=random_unit(w); v.pos.x+=u*dst.x; v.pos.y+=u*dst.y; v.pos.z+=u*dst.z;
        extra(w)->effect_texture(slot,w,2); extra(w)->effect_set(slot,&v);
    }
}
static float acid_shape(int f)
{
    unsigned i;
    for (i=0;i<6;++i) if (f<=ShapeTbl_Acid[i].key) {
        if (f==ShapeTbl_Acid[i].key || i==0) return ShapeTbl_Acid[i].cnt;
        return ShapeTbl_Acid[i-1].cnt+(ShapeTbl_Acid[i].cnt-ShapeTbl_Acid[i-1].cnt)*
            (f-ShapeTbl_Acid[i-1].key)/(ShapeTbl_Acid[i].key-ShapeTbl_Acid[i-1].key);
    }
    return 0; /* Unerreichbar fuer gueltige Motion 6; Aufrufer prueft 999. */
}
static void move(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p; int32_t n; float dx,dz;
    switch (w->mode2) {
    case 0:
        if (!w->mode3) { w->flg=(w->flg|0x100000u)&~0x80000u; w->mtn_no=0;
            w->frm_no=0; w->mtn_add=65536; ++w->mode3; w->ct0=10; }
        if (w->mode3==1) {
            if (w->ct0) { --w->ct0; break; }
            w->mode1=1; if (!player(w,&p)) return;
            dx=p.pos.x-w->px; dz=p.pos.z-w->pz;
            if (njSqrt(dx*dx+dz*dz)>9.0f || !in_cone(w,&p,7281)) mode(w,1,1,1,0);
        }
        break;
    case 1:
        if (!w->mode3) { w->flg|=0x1c0000u; if (w->mtn_no!=1) { w->mtn_no=1; w->frm_no=0; }
            w->mtn_add=65536; ++w->mode3; }
        if (w->mode3==1) {
            if (!player(w,&p)) return; pursue(w,&p); dx=p.pos.x-w->px; dz=p.pos.z-w->pz;
            if (njSqrt(dx*dx+dz*dz)<5.0f && in_cone(w,&p,5461)) mode(w,1,1,0,0);
        }
        break;
    case 2:
        if (!w->mode3) {
            if (!frames(w,6,&n)) return;
            w->flg=(w->flg&~0x100000u)|0xc0000u; w->mtn_no=6; w->frm_no=0;
            w->mtn_add=65536; w->ct0=n-2;
            EXTRA(w,morph_sources,"bhEne12_MV02: obj_a/obj_b"); extra(w)->morph_sources(w,0,1);
            w->mdl_no=0; w->mdflg|=2u; ++w->mode3; break;
        }
        if (w->mode3==1) {
            if (w->frm_no/65536>999 || w->frm_no<0) { missing(w,"bhEne_GetShapeCnt(en12): key"); return; }
            w->shp_ct=acid_shape(w->frm_no/65536);
            if (!w->frm_no) { w->mode1=1; w->mode2=1; w->mode3=0; w->mtn_no=1; w->mdflg&=~2u; }
            if (w->frm_no>=0x1e0000 && w->frm_no<0x280001) acid(w);
            if (w->frm_no>=0x280000) w->flg&=~0x400000u;
            if (!player(w,&p)) return; pursue(w,&p);
        }
        break;
    case 3:
    case 4:
        if (!w->mode3) {
            int kind=w->mode2;
            if (!frames(w,kind==3?4:5,&n)) return;
            w->flg|=0x1c0000u; w->mtn_no=(uint32_t)(kind==3?4:5);
            w->frm_no=0; w->mtn_add=65536; w->ct1=0;
            if (kind==3) { ++w->mode3; w->ct0=n-2; break; }
            w->gpx=w->px; w->gpy=w->py; w->gpz=w->pz;
            if (!player(w,&p)) return;
            /* ELF 1D6900..1D6A38 bestaetigt den Decomp-Fehler NICHT als
               sicheren Default: mode2 ausserhalb 3..12 benutzt ungesetzte
               f20/f2. Keine geratene Spielerposition/Distanz als Ersatz. */
            if (p.mode2>=3 && p.mode2<=12) {
                float prediction=p.mode2<=5?-15.0f:(p.mode2<=8?-25.0f:10.0f);
                dx=p.pos.x+prediction*njSin(p.ay)-w->px;
                dz=p.pos.z+prediction*njCos(p.ay)-w->pz;
                w->ofx=njSqrt(dx*dx+dz*dz);
                if (!(w->ofx<=45.0f)) w->ofx=45.0f;
                if (w->ofx<20.0f) w->ofx=20.0f;
            } else { missing(w,"bhEne12_MV04: original uninitialised f20/f2 (1D6A18)"); return; }
            /* en12.c:987 mode3 += 1 erst NACH dem ofx-Abstand: ein Abbruch
               oben laesst MV04 in Fall 0, statt mit altem ofx weiterzulaufen. */
            ++w->mode3;
        }
        if (w->mode2==4 && w->mode3==1) {
            if (!frames(w,w->mtn_no,&n)) return; w->ct0=n-2; w->mtn_add=65536; ++w->mode3; break;
        }
        if ((w->mode2==3 && w->mode3==1) || (w->mode2==4 && w->mode3==2)) {
            int radial=w->mode2==4, i;
            int lower=radial?0xe0000:0xf0000;
            if (w->frm_no>=lower && w->frm_no<0x110001) {
                int count=radial?(int)(6.0f+0.2f*(w->ofx-20.0f)):8;
                for (i=0;i<count;++i) {
                    RECVX_ENEMY_VEC v; int a=w->ay, time;
                    if (radial) {
                        float step=182.04445f*(30/count);
                        float jitter=(int)step*random_unit(w);
                        a=(int)((float)(w->ay+10922-w->ct1*(int)step)-jitter);
                        v.x=0; v.z=-w->ofx-2.0f*random_unit(w);
                        time=(int)(((double)w->ct1*1.5)/count+40.0);
                    } else {
                        v.x=4.0f-2.0f*random_unit(w);
                        v.z=(-20.0f-2.2f*w->ct1)-2.2f*random_unit(w);
                        time=(int)((double)w->ct1/2.5+40.0);
                    }
                    v.y=-1; v=rotate(v,a);
                    v.x+=radial?w->gpx:w->px; v.y+=radial?w->gpy:w->py; v.z+=radial?w->gpz:w->pz;
                    flame_liquid(w,v,time); ++w->ct1;
                }
            }
            if (w->frm_no>=0x110000) w->flg&=~0x400000u;
            if (!w->frm_no) { w->mode1=1; w->mode2=1; w->mode3=0; w->mtn_no=1; }
            if (!player(w,&p)) return; pursue(w,&p);
            if (w->frm_no==0x50000) fire_hand(w,0);
            if (!w->frm_no) fire_hand(w,1);
            if (w->frm_no>=0xa0000 && w->frm_no<0x140001) fire_hand(w,2);
        }
        break;
    case 5: break; /* MV05 ist auch in der Referenz leer. */
    case 6:
        if (!w->mode3) { w->flg=(w->flg|0x100000u)&~0x80000u; w->mtn_no=0;
            w->hokan_count=20; w->frm_no=0; ++w->mode3; }
        break;
    case 7:
        if (!w->mode3) {
            RECVX_ENEMY_VEC q; int32_t a[3];
            NEED(w,ground,"bhGetGroundPosition(en12 MV07)"); NEED(w,motion_root_angles,"MN_WORK angle(en12 MV07)");
            if (!parts(w,&joint_tree[0][0],&q)) return;
            put_position(w,q); w->py+=5.0f; q=position(w); w->py=w->sv->ground(&q); w->mtn_md|=0x100u;
            if (!w->sv->motion_root_angles(w,w->mtn_no,(uint32_t)(w->frm_no/65536),a)) { missing(w,"MN_WORK angle(en12 MV07)"); return; }
            w->ay=a[1]; w->flg=(w->flg|0x140000u)&~0x80000u;
            w->mtn_room=0; w->mtn_no=0; w->frm_no=0; w->mtn_add=65536; w->hokan_count=10; ++w->mode3;
            NEED(w,system_flags_or,"rm_flg(en12 MV07)"); w->sv->system_flags_or(1,1);
        }
        break;
    default: missing(w,"bhEne12_MoveMode2"); return;
    }
    if (w->flg&4u) { init_damage(w); return; }
    if (w->mode1) {
        if (w->type==0) brain(w);
        else if (w->type==1) {
            /* BrainType[1] appelle MV00, NICHT den Move-Wrapper rekursiv. */
            if (!w->mode3) { w->flg=(w->flg|0x100000u)&~0x80000u; w->mtn_no=0; w->frm_no=0;
                w->mtn_add=65536; ++w->mode3; w->ct0=10; }
            if (w->mode3==1) {
                if (w->ct0) --w->ct0;
                else { w->mode1=1; if (!player(w,&p)) return; dx=p.pos.x-w->px; dz=p.pos.z-w->pz;
                    if (njSqrt(dx*dx+dz*dz)>9.0f || !in_cone(w,&p,7281)) mode(w,1,1,1,0); }
            }
        } else missing(w,"bhEne12_BrainType");
    }
}

/* zonzon1.c:725 mit vollstaendig nullem CombJointTbl[33] (en12.c:294):
   keine Gelenkkorrektur, jedes Gelenk traegt die Punkte unveraendert. */
static int calc_damage(RECVX_ENEMY_WORK* w)
{
    const EN12_COMBO* c; uint32_t i,n; RECVX_ENEMY_VEC dv;
    if (w->wpnr_no<0 || w->wpnr_no>=21) { missing(w,"bhEne_CalcDamage(en12): weapon"); return 0; }
    if (!w->sv || !w->sv->obj_num || !(n=w->sv->obj_num(w))) { missing(w,"bhEne_CalcDamage(en12): obj_num"); return 0; }
    if (n>33u) { missing(w,"bhEne_CalcDamage(en12): CombJointTbl bounds"); return 0; }
    c=&CombWepTbl[w->wpnr_no]; w->comb_flg&=~0xdu; w->total_dam=0;
    for (i=1;i<n;++i) if (w->dam[i]) {
        if (w->comb_wep!=(uint32_t)w->wpnr_no || !w->comb_timeout) {
            w->comb_wep=(uint32_t)w->wpnr_no; w->comb_pnt=0;
        }
        w->comb_timeout=c->timeout;
        if (w->comb_flg&0x10u) w->comb_pnt+=c->pt[0];
        if (w->comb_flg&0x20u) w->comb_pnt+=c->pt[1];
        if (w->comb_flg&0x40u) w->comb_pnt+=c->pt[2];
        if (c->crit && w->comb_pnt>=c->crit) {
            w->comb_flg|=1u; if (!(w->comb_flg&2u)) w->comb_pnt=0;
            w->total_dam+=w->dam[i]+w->dam[i]*c->bonus/100;
        } else w->total_dam+=w->dam[i];
    }
    dv.x=w->dvx; dv.y=w->dvy; dv.z=w->dvz; dv=rotate(dv,-w->ay);
    w->comb_flg|=dv.z>=0.0f?4u:8u; return 1;
}
static RECVX_ENEMY_VEC blood_offset(RECVX_ENEMY_WORK* w, const EN12_BLOOD* b, int mirror)
{
    RECVX_ENEMY_VEC p=b->ofp;
    if (mirror && !(w->comb_flg&4u)) p.z=-p.z;
    p.x+=b->rx*random_unit(w)-b->rx/2.0f;
    p.y+=b->ry*random_unit(w)-b->ry/2.0f;
    p.z+=b->rz*random_unit(w)-b->rz/2.0f; return p;
}
static void blood(RECVX_ENEMY_WORK* w, int count)
{
    RECVX_ENEMY_PLAYER p; RECVX_EN12_EFFECT_TEMPLATE t={0}; RECVX_EN12_EFFECT_VIEW v;
    int base,i,slot;
    if (extension(w)->surface_slot==-2) { missing(w,"bhEne12_Blood: exp1 = &eff[-1] (en12.c:1939)"); return; }
    EXTRA(w,effect_tb,"bhSetEffectTb(en12 Blood)"); EXTRA(w,effect_get,"eff owner(en12 Blood)"); EXTRA(w,effect_set,"eff owner(en12 Blood)");
    if (!player(w,&p)) return; base=arc(w->px-p.pos.x,w->pz-p.pos.z);
    t.id=265; t.flg=1; t.type=1;
    for (i=0;i<count;++i) {
        float r=2.0f+5.0f*random_unit(w), s; int a=(int)((base+21845.0f*random_unit(w))-10922.0f);
        t.pos.x=w->px-r*njSin(a); t.pos.y=w->py+0.01f; t.pos.z=w->pz-r*njCos(a);
        s=0.2f+0.8f*random_unit(w); t.scale.x=s; t.scale.y=1; t.scale.z=s;
        slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG|RECVX_EN12_EF_TYPE|RECVX_EN12_EF_POS|RECVX_EN12_EF_SX|RECVX_EN12_EF_SY|RECVX_EN12_EF_SZ,0,0,0);
        if (slot<0) continue;
        if (!extra(w)->effect_get(slot,&v)) { missing(w,"eff slot(en12 Blood)"); return; }
        v.normal.x=0; v.normal.y=1; v.normal.z=0; v.exp1_slot=extension(w)->surface_slot; v.ct0=60;
        extra(w)->effect_set(slot,&v);
    }
}
static void hit_mark(RECVX_ENEMY_WORK* w)
{
    const EN12_REACT* r; const EN12_BLOOD* b; int range=0,i; RECVX_ENEMY_VEC o;
    NEED(w,random_next,"rand(en12 HitMark)");
    if (w->wpnr_no<0 || w->wpnr_no>=21 || w->djnt_no<0 || w->djnt_no>=22) { missing(w,"bhEne12_HitMark: table bounds"); return; }
    r=&DmgReact[w->wpnr_no]; b=&BloodTbl[w->djnt_no];
    if (w->comb_flg&0x20u) range=1; if (w->comb_flg&0x40u) range=2;
    if (r->type[range]>=0) {
        /* Diese drei RNG-Aufrufe auch bei Welt-Hitpunktwaffen erhalten. */
        o=blood_offset(w,b,1);
        NEED(w,blood_effect_burst,"bhEne_SetBloodEffectBurst(en12)");
        if (w->wpnr_no==10 || w->wpnr_no>=13) {
            RECVX_ENEMY_VEC hit={w->dpx,w->dpy,w->dpz};
            w->sv->blood_effect_burst(w,r->type[range],w->djnt_no,&hit,1);
        } else w->sv->blood_effect_burst(w,r->type[range],w->djnt_no,&o,0);
        if (w->type!=1 && r->bloodstain[range]) blood(w,r->bloodstain[range]);
    }
    if ((r->exef&1) && !b->flg) {
        NEED(w,fire_effect,"bhEne_SetFireEffect(en12 HitMark)");
        for (i=0;i<4;++i) {
            float size; int life;
            o=blood_offset(w,b,0); size=0.5f+0.5f*random_unit(w); life=(int)(40.0f*random_unit(w))+20;
            w->sv->fire_effect(w,w->djnt_no,&o,size,life);
        }
    }
    if (r->exef&2) { NEED(w,acid_effect,"bhEne_SetAcidEffect(en12 HitMark)");
        o=blood_offset(w,b,0); w->sv->acid_effect(w,w->djnt_no,&o,2.0f); }
}
static void init_damage(RECVX_ENEMY_WORK* w)
{
    w->flg&=~4u; if (!calc_damage(w)) return;
    if (w->wpnr_no==16 && !(w->flg2&4u) && w->comb_pnt!=1) return;
    w->hp-=w->total_dam;
    if (w->wpnr_no==17 && !(w->flg2&4u)) return;
    hit_mark(w);
    if (w->hp<0) mode(w,4,0,0,0);
    else if (!(w->flg&0x400000u) && (w->total_dam>=21 || (w->comb_flg&1u))) mode(w,3,0,0,0);
}
static void null_translation(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* table, uint32_t count)
{
    int f=w->frm_no/65536; RECVX_ENEMY_VEC v;
    if (f<0 || (uint32_t)f>=count) { missing(w,"bhEne_AddNullTrans(en12): table bounds"); return; }
    v=rotate(table[f],w->ay); w->px+=v.x; w->pz+=v.z;
}
static void damage(RECVX_ENEMY_WORK* w)
{
    int32_t n;
    if (w->flg&4u) {
        w->flg&=~4u; if (!calc_damage(w)) return;
        if (w->wpnr_no!=16 || (w->flg2&4u) || w->comb_pnt==1) {
            w->hp-=w->total_dam;
            if (w->wpnr_no!=17 || (w->flg2&4u)) hit_mark(w);
        }
    }
    if (w->mode2!=0) { missing(w,"bhEne12_DamageMode2"); return; }
    if (!w->mode3) {
        RECVX_ENEMY_VEC v={w->dvx,w->dvy,w->dvz}; v=rotate(v,-w->ay);
        w->flg&=~0x180000u;
        if (!(v.z>=0.0f)) { w->mtn_no=13; w->flg&=~0x40000u; w->mtn_md&=~2u; }
        else {
            if (w->mtn_no==1 || (w->mtn_no>=4 && w->mtn_no<=6)) {
                if (w->frm_no>=0x60000 && w->frm_no<0x230001) w->mtn_md|=2u;
                else w->mtn_md&=~2u;
            }
            w->mtn_no=12;
        }
        if (!frames(w,w->mtn_no,&n)) return;
        w->frm_no=0; w->mtn_add=65536; w->hokan_count=10; w->hokan_rate=45875;
        w->ct0=n-1; ++w->mode3;
    }
    if (w->mode3==1) {
        if (w->mtn_no==13) null_translation(w,vm1_013,57);
        if (w->ct0--==0) {
            int selection=(int)(3.0f*random_unit(w));
            mode(w,1,0,selection==0?3:(selection==1?4:2),0);
            w->mtn_no=selection==0?4u:(selection==1?5u:6u); w->frm_no=0; w->hokan_count=15;
            w->mtn_md&=~2u; w->flg&=~4u;
            memset(extension(w)->wait,0,4*sizeof(int32_t)); /* +20 bleibt erhalten */
            w->flg|=0x400000u;
        }
    }
}
static void die(RECVX_ENEMY_WORK* w)
{
    int32_t n;
    if (w->mode2!=0) { missing(w,"bhEne12_DieMode2"); return; }
    if (!w->mode3) {
        if (!frames(w,14,&n)) return;
        w->flg&=~0x1c0060u; w->mtn_no=14; w->frm_no=0; w->mtn_add=65536;
        w->hokan_count=10; w->ct0=n-1; ++w->mode3;
    }
    if (w->mode3==1) { null_translation(w,vm1_014,150);
        if (w->ct0--==0) { w->mtn_add=0; ++w->mode3; w->flg|=2u; } }
}
static void nage(RECVX_ENEMY_WORK* w)
{
    static const RECVX_ENEMY_VEC target[2]={{-0.000011f,-0.00069f,-9.8f},{-0.00001f,0.108913f,-9.8f}};
    RECVX_ENEMY_PLAYER p; int32_t n; RECVX_ENEMY_VEC v; int i;
    NEED(w,player_set,"bhEne12_NG00: player owner"); if (!player(w,&p)) return;
    if (w->mode2!=0) { missing(w,"bhEne12_NageMode2"); return; }
    if (!w->mode3) {
        if (!frames(w,8,&n)) return;
        w->flg=(w->flg&~0xc0040u)|0x100000u;
        w->mtn_no=8; w->frm_no=0; w->mtn_add=65536; w->ct0=n-1;
        p.mode0=6; p.mode3=0;
        { int a=(int16_t)(w->ay-p.ay); p.mode2=(uint8_t)((a>16384 || a< -16384)?0:1); }
        w->ayp=arc(w->px-p.pos.x,w->pz-p.pos.z); w->ct1=8; w->ct2=0;
        p.exp1_flags&=~1u; w->sv->player_set(&p);
        EXTRA(w,player_texture_zero,"bhEne12_NG00: plp txp[0]"); extra(w)->player_texture_zero(); ++w->mode3;
    }
    if (w->mode3!=1) return;
    if (w->ct0--==0) {
        if (!parts(w,&joint_tree[0][0],&v)) return;
        w->px=v.x; w->pz=v.z; w->flg|=0x100000u; w->mtn_no=0; w->frm_no=0; ++w->mode3; return;
    }
    if (w->frm_no<0x640000) {
        if (p.mode2>1) { missing(w,"bhEne12_NG00: player mode2"); return; }
        v=rotate(target[p.mode2],w->ayp+32768);
        w->xn=v.x+p.pos.x; w->yn=v.y+p.pos.y; w->zn=v.z+p.pos.z;
        if (w->ct1) {
            w->ay+=(int16_t)(w->ayp-w->ay)/w->ct1;
            w->px+=(w->xn-w->px)/w->ct1; w->pz+=(w->zn-w->pz)/w->ct1; --w->ct1;
        } else { w->px=w->xn; w->pz=w->zn; }
    }
    if (w->frm_no==0x3c0000) { p.flg|=2u; w->sv->player_set(&p); }
    if (w->frm_no==0x410000) {
        EXTRA(w,player_material,"npSetAllMatColor(plp)"); EXTRA(w,object_material,"npSetAllMatColor(obwp)");
        extra(w)->player_material(0xff402020u);
        extra(w)->object_material(0,1,0xff402020u); extra(w)->object_material(1,1,0xff402020u);
    }
    if (w->frm_no>0x410000) {
        EXTRA(w,player_fire,"bhEne_SetFireEffect(plp)");
        for (i=0;pc_eff_tab[i].joint;++i) if (pc_eff_tab[i].delay==w->ct2)
            extra(w)->player_fire(pc_eff_tab[i].joint,&pc_eff_tab[i].offset,pc_eff_tab[i].size,pc_eff_tab[i].life);
        ++w->ct2;
    }
}
static void fixed_legs(RECVX_ENEMY_WORK* w)
{
    int f; RECVX_ENEMY_VEC d; RECVX_EN12_EXT* e=extension(w); int choose=0;
    if (!(w->flg&0x40000u) || w->mtn_room) return;
    f=w->frm_no/65536;
    if (w->mtn_no==1 || (w->mtn_no>=4 && w->mtn_no<=6)) {
        choose=(f>=17 && f<=47); if (w->mtn_md&2u) choose=!choose;
        e->foot_chain=choose?&joint_tree[5][1]:&joint_tree[2][2];
    } else if (w->mtn_no==12) {
        choose=f<=12; if (w->mtn_md&2u) choose=!choose;
        e->foot_chain=choose?&joint_tree[2][2]:&joint_tree[5][1];
    }
    if (!e->foot_chain) return;
    EXTRA(w,fix_offset,"bhCalcFixOffset(en12)");
    if (!extra(w)->fix_offset(w,e->foot_chain,&d)) { missing(w,"bhCalcFixOffset(en12): pose"); return; }
    w->px-=d.x; w->pz-=d.z;
}
static void check_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC j; float dx,dy,dz;
    NEED(w,check_wall,"bhCheckWall(en12)"); if (!joint(w,5,&j)) return;
    dx=j.x-w->px; dy=j.y-w->py; dz=j.z-w->pz;
    w->px+=dx; w->py+=dy; w->pz+=dz; w->sv->check_wall(w);
    w->px-=dx; w->py-=dy; w->pz-=dz; w->sv->check_wall(w);
}
static void look(RECVX_ENEMY_WORK* w)
{
    NJS_MATRIX desired,current,parent,inv,turn; NJS_VECTOR view,base,axis;
    float head[16],ph[16]; float cross; int32_t a[3],rx,ry,rz,z1,z2,angle;
    RECVX_EN12_EXT* e=extension(w);
    if (w->mtn_room) { w->flg&=~0x100000u; return; }
    if (!(w->flg&0x100000u) && !e->look_count) return;
    NEED(w,joint_matrix,"bhEne12_LookPlayaer: owP"); EXTRA(w,joint_pose_set,"bhEne12_LookPlayaer: objP/owP setter");
    if (!w->sv->joint_matrix(w,4,parent) || !w->sv->joint_matrix(w,5,head)) { missing(w,"bhEne12_LookPlayaer: joint"); return; }
    memcpy(inv,parent,sizeof(inv)); njInvertMatrix(&inv);
    njUnitMatrix(&desired);
    if (!(w->flg&0x100000u)) {
        EXTRA(w,motion_angles,"bhEne12_LookPlayaer: md2P[5].p[1]");
        if (!extra(w)->motion_angles(w,w->mtn_no,5,(uint32_t)(w->frm_no/65536),a)) { missing(w,"bhEne12_LookPlayaer: angle key"); return; }
        njRotateXYZ(&desired,a[0],(w->mtn_md&2u)?-a[1]:a[1],(w->mtn_md&2u)?-a[2]:a[2]);
    } else {
        NEED(w,player_joint_matrix,"bhEne12_LookPlayaer: player joint");
        if (!w->sv->player_joint_matrix(5,ph)) { missing(w,"bhEne12_LookPlayaer: player pose"); return; }
        view.x=ph[12]-head[12]; view.y=ph[13]-head[13]; view.z=ph[14]-head[14];
        njUnitVector(&view); njCalcVector(&inv,&view,&view);
        rx=(int)(10430.381f*asinf(view.y)); ry=arc(-view.x,-view.z);
        if (ry>10922) ry=10922; if (ry< -10922) ry=-10922;
        if (rx>10922) rx=10922; if (rx< -5461) rx=-5461;
        base.x=0; base.y=0; base.z=-1;
        njUnitMatrix(&turn); njRotateXYZ(&turn,rx,ry,0); njCalcVector(&turn,&base,&view);
        cross=njOuterProduct(&base,&view,&axis); njUnitVector(&axis);
        angle=(int)(10430.381f*asinf(cross)); njRotate(&desired,&axis,angle);
        njRotateZ(&desired,-(int)(10430.381f*asinf(desired[1]))); e->look_count=16;
    }
    memcpy(current,inv,sizeof(current));
    memcpy(turn,head,sizeof(turn)); njMultiMatrix(&current,&turn);
    z2=(int)(10430.381f*asinf(desired[1])); z1=(int)(10430.381f*asinf(current[1]));
    base.x=current[8]; base.y=current[9]; base.z=current[10];
    view.x=desired[8]; view.y=desired[9]; view.z=desired[10];
    cross=njOuterProduct(&base,&view,&axis); njUnitVector(&axis);
    angle=(int)(10430.381f*asinf(cross))/e->look_count;
    njUnitMatrix(&turn); njRotate(&turn,&axis,angle); njMultiMatrix(&turn,&current); memcpy(desired,turn,sizeof(desired));
    njRotateZ(&desired,(int16_t)(z2-z1)/e->look_count);
    rx=arc(desired[6],desired[5]); ry=arc(-desired[2],desired[0]); rz=(int)(10430.381f*asinf(desired[1]));
    a[0]=rx; a[1]=ry; a[2]=rz;
    njRotateXYZ(&parent,rx,ry,rz); extra(w)->joint_pose_set(w,5,a,parent);
    if (!(w->flg&0x100000u) && e->look_count) --e->look_count;
}

static void fire_hand(RECVX_ENEMY_WORK* w, int action)
{
    RECVX_EN12_EFFECT_TEMPLATE t={0}; RECVX_EN12_EFFECT_VIEW v;
    RECVX_EN12_EXT* e=extension(w); RECVX_ENEMY_VEC off={0,0.5f,0}, hand;
    int i,slot;
    if (action==3) {
        NEED(w,joint_point,"bhEne12_SetFireBintaEffect: hand point");
        if (!w->sv->joint_point(w,9,&off,&hand)) { missing(w,"bhEne12_SetFireBintaEffect: hand pose"); return; }
        e->previous_hand=hand; return;
    }
    EXTRA(w,effect_get,"eff owner(en12 FireBinta)"); EXTRA(w,effect_set,"eff owner(en12 FireBinta)");
    if (action==1) {
        for (i=0;i<16;++i) if (e->hand_slot[i]>=0) {
            if (!extra(w)->effect_get(e->hand_slot[i],&v)) { missing(w,"eff slot(en12 FireBinta)"); continue; }
            if ((v.flg&1u) && v.id==274) { v.mode0=3; extra(w)->effect_set(e->hand_slot[i],&v); }
        }
        return;
    }
    EXTRA(w,effect_tb,"bhSetEffectTb(en12 FireBinta)");
    if (!action) {
        t.id=274; t.flg=1; t.type=0; t.ay=0; t.mdlver=0;
        for (i=0;i<16;++i) {
            t.scale.x=t.scale.y=alex_eff_tab[i].size;
            slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG|RECVX_EN12_EF_TYPE|RECVX_EN12_EF_POS|RECVX_EN12_EF_AY|RECVX_EN12_EF_MDLVER|RECVX_EN12_EF_SX|RECVX_EN12_EF_SY,
                w,&alex_eff_tab[i].offset,alex_eff_tab[i].joint);
            /* Bei vollem Pool bleibt ein vorhandener Zeiger erhalten. */
            if (slot<0) continue;
            e->hand_slot[i]=slot;
            if (!extra(w)->effect_get(slot,&v)) { missing(w,"eff slot(en12 FireBinta)"); return; }
            v.ct0=alex_eff_tab[i].life; v.ct1=alex_eff_tab[i].delay; extra(w)->effect_set(slot,&v);
        }
    } else if (action==2) {
        RECVX_ENEMY_VEC delta,unit; NJS_VECTOR norm;
        static const float base[3]={0.8f,0.6f,0.5f}, span[3]={0.4f,0.6f,0.7f};
        NEED(w,joint_point,"bhEne12_SetFireBintaEffect: hand point");
        if (!w->sv->joint_point(w,9,&off,&hand)) { missing(w,"bhEne12_SetFireBintaEffect: hand pose"); return; }
        delta.x=0.25f*(e->previous_hand.x-hand.x); delta.y=0.25f*(e->previous_hand.y-hand.y); delta.z=0.25f*(e->previous_hand.z-hand.z);
        norm.x=delta.x; norm.y=delta.y; norm.z=delta.z; njUnitVector(&norm);
        unit.x=0.3f*norm.x; unit.y=0.3f*norm.y; unit.z=0.3f*norm.z;
        t.id=275; t.flg=1; t.type=0; t.ay=0; t.mdlver=0; t.pos=hand;
        for (i=0;i<3;++i) {
            t.scale.x=t.scale.y=base[i]+span[i]*random_unit(w);
            slot=extra(w)->effect_tb(&t,RECVX_EN12_EF_ID|RECVX_EN12_EF_FLG|RECVX_EN12_EF_TYPE|RECVX_EN12_EF_AY|RECVX_EN12_EF_MDLVER|RECVX_EN12_EF_POS|RECVX_EN12_EF_SX|RECVX_EN12_EF_SY,0,0,0);
            if (slot>=0) {
                if (!extra(w)->effect_get(slot,&v)) { missing(w,"eff slot(en12 FireBinta)"); return; }
                v.velocity=unit; extra(w)->effect_set(slot,&v);
            }
            if (i<2) { t.pos.x+=delta.x; t.pos.y+=delta.y; t.pos.z+=delta.z; }
        }
    }
}
static void se(RECVX_ENEMY_WORK* w, int number)
{
    RECVX_ENEMY_VEC p=position(w);
    if (w->flg&0x10000u) return;
    NEED(w,enemy_se,"RequestEnemySe(en12)");
    /* sys->enow ist der Passindex des Dispatchers (eneset.c:360); der
       Slotindex idx_ct ist ein anderer Wert und ersetzt ihn nicht. */
    NEED(w,enemy_now,"RequestEnemySe(en12): sys->enow");
    w->sv->enemy_se(w->sv->enemy_now(),&p,number);
}
static void foot_se(RECVX_ENEMY_WORK* w, int foot)
{
    RECVX_ENEMY_VEC p; RECVX_ENEMY_ATR atr; int hit;
    if (w->mtn_md&2u) foot=!foot;
    if (!parts(w,foot?&joint_tree[2][2]:&joint_tree[5][1],&p)) return;
    EXTRA(w,floor_enemy,"bhCheckFloorEnemy(en12 FootSE)");
    hit=extra(w)->floor_enemy(w->flr_no,p.x,p.z,&atr);
    se(w,(hit && atr.prm0==12)?0x1230c:0x12300);
}
static void call_se(RECVX_ENEMY_WORK* w)
{
    int f=w->frm_no;
    if (w->mtn_room) return;
    switch (w->mtn_no) {
    case 1:
        if (f==0x120000) foot_se(w,0); if (f==0x300000) foot_se(w,1); break;
    case 4: case 5:
        if (f==0x120000) foot_se(w,0); if (f==0x300000) foot_se(w,1);
        if (f==0xf0000) {
            RECVX_ENEMY_VEC p=position(w); se(w,0x12301);
            /* Effektkanal 127 hat KEIN flg-0x10000-Gate (zonzon1.c:846). */
            NEED(w,enemy_se,"bhEne_CallEffectSE(en12)"); w->sv->enemy_se(127,&p,0x12302);
        }
        break;
    case 6:
        if (f==0x120000) foot_se(w,0); if (f==0x1e0000) se(w,0x12304); break;
    case 8:
        if (f==0x80000) foot_se(w,1); if (f==0x400000) se(w,0x12306); break;
    case 12:
        if (!f) se(w,0x1002307); if (f==0x90000) foot_se(w,0); break;
    case 13:
        if (!f) se(w,0x1002308); if (f==0xc0000) foot_se(w,0); if (f==0x160000) foot_se(w,1); break;
    case 14:
        if (!f) se(w,0x1002309); if (f==0x190000) se(w,0x230a); if (f==0x800000) se(w,0x230b); break;
    }
}
static void control_player(RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_PLAYER p; int32_t ct1,ayp; uint32_t n;
    if (!player(w,&p)) return;
    if ((p.mode0!=4 && p.mode0!=6) || p.mode2>1) return;
    NEED(w,player_set,"bhEne12_PlayerControl: player owner");
    EXTRA(w,player_library,"bhEne12_PlayerControl: mnwP");
    if (p.mode0==4) {
        switch (p.mode3) {
        case 0:
            NEED(w,player_voice,"bhEne_CallPlayerVoice(en12)"); NEED(w,vibration,"StartVibrationEx(en12)");
            p.flg&=~0x40000u; p.flg2|=1u; p.mtn_no=p.mode2?72:71;
            p.frm_no=0; p.hokan_count=3; p.hokan_rate=32768; p.mtn_add=65536; ++p.mode3;
            w->sv->player_set(&p); w->sv->player_voice(0x402); w->sv->vibration(1,9); return;
        case 1:
            if (!p.frm_no) {
                p.mtn_enemy=1; p.mtn_enemy_owner=(uint32_t)w->idx_ct;
                p.mtn_no=p.mode2?23:22; p.frm_no=0; p.hokan_count=3;
                p.hokan_rate=32768; p.mtn_add=65536; ++p.mode3;
                w->sv->player_set(&p); extra(w)->player_library(w,1);
            }
            return;
        case 2:
            if (!p.frm_no) {
                /* La reference installe epw->mnwPb, pas plp->mnwPb. */
                p.mtn_enemy=1; p.mtn_enemy_owner=(uint32_t)w->idx_ct;
                p.flg=(p.flg&~0x10004u)|8u; p.flg2&=~1u; p.at_flg=0; p.stflg&=~0x10000u;
                p.mode0=1; p.mode1=p.mode2=p.mode3=0;
                w->sv->player_set(&p); extra(w)->player_library(w,2);
            }
            return;
        default: return;
        }
    }
    EXTRA(w,player_extra_get,"bhEne12_PlayerControl: plp ct1/ayp");
    EXTRA(w,player_extra_set,"bhEne12_PlayerControl: plp ct1/ayp");
    EXTRA(w,player_wall,"bhCheckWall(plp, en12)");
    if (!extra(w)->player_extra_get(&ct1,&ayp)) { missing(w,"bhEne12_PlayerControl: player extra"); return; }
    switch (p.mode3) {
    case 0:
        p.flg=(p.flg&~0xc0000u)|0x10004u; p.stflg|=0x10000u;
        p.exp1_flags=(p.exp1_flags|0x1e0u)&~4u;
        p.ct0=p.mode2?8:1;
        ayp=p.mode2?arc(w->px-p.pos.x,w->pz-p.pos.z):arc(p.pos.x-w->px,p.pos.z-w->pz);
        ct1=8; p.mtn_enemy=1; p.mtn_enemy_owner=(uint32_t)w->idx_ct;
        p.mtn_no=p.mode2+20u; p.frm_no=0; p.hokan_count=5; p.hokan_rate=52428;
        p.mtn_add=0; ++p.mode3;
        w->sv->player_set(&p); extra(w)->player_library(w,1);
        /* fall through, ct0-- auf dem Initialisierungspass */
    case 1:
        if (p.ct0--==0) {
            NEED(w,player_motion_frames,"bhEne12_PlayerControl: player frm_num");
            n=w->sv->player_motion_frames(p.mtn_no,1);
            if (!n) { missing(w,"bhEne12_PlayerControl: missing player motion"); return; }
            p.mtn_add=65536; p.ct0=(int32_t)n-1; ++p.mode3;
        }
        if (ct1) { p.ay+=(int16_t)(ayp-p.ay)/ct1; --ct1; }
        break;
    case 2:
        if (ct1) { p.ay+=(int16_t)(ayp-p.ay)/ct1; --ct1; }
        if (p.frm_no==0x3c0000) { NEED(w,vibration,"StartVibrationEx(en12 grab)"); w->sv->vibration(1,11); }
        if (!p.frm_no) {
            NEED(w,player_motion_frames,"bhEne12_PlayerControl: player frm_num");
            n=w->sv->player_motion_frames(p.mtn_no,1);
            if (!n) { missing(w,"bhEne12_PlayerControl: missing player motion"); return; }
            p.frm_no=(int32_t)(65536.0f*(float)(n-1)); p.mtn_add=0; ++p.mode3;
        }
        break;
    }
    w->sv->player_set(&p); extra(w)->player_extra_set(ct1,ayp);
    /* Auch mode3 >= 3 macht den Wandpass am Kopf, exakt wie en12.c:1757. */
    {
        float m[16],dx,dz;
        NEED(w,player_joint_matrix,"bhEne12_PlayerControl: player owP[5]");
        if (!w->sv->player_joint_matrix(5,m)) { missing(w,"bhEne12_PlayerControl: player pose"); return; }
        dx=m[12]-p.pos.x; dz=m[14]-p.pos.z;
        p.pos.x+=dx; p.pos.z+=dz; w->sv->player_set(&p);
        extra(w)->player_wall(&p);
        p.pos.x-=dx; p.pos.z-=dz; w->sv->player_set(&p);
    }
}

void recvx_en12(RECVX_ENEMY_WORK* w)
{
    if (!w) return;
    if (!w->exp0 && !recvx_en12_bind(w,0)) return;
    if (!w->sv || !w->sv->random_next) { missing(w,"rand(en12)"); return; }
    if (w->mode0!=5) { w->flg&=~0x100u; w->mtn_md=(w->mtn_md|0x10u)&~0x20u; }
    if (w->type==1 && (w->flg&4u)) {
        w->flg&=~4u; hit_mark(w); mode(w,1,0,7,0); w->type=0;
    }
    switch (w->mode0) {
    case 0: init(w); break;
    case 1: move(w); break;
    case 2: nage(w); break;
    case 3: damage(w); break;
    case 4: die(w); break;
    case 5: NEED(w,event,"bhEne_Event(en12)"); w->sv->event(w); break;
    default: missing(w,"bhEne12_Mode0"); return;
    }
    call_se(w);
    NEED(w,set_motion,"bhSetMotion(en12)"); w->sv->set_motion(w,w->mtn_add,w->mtn_md);
    fixed_legs(w);
    NEED(w,weapon_atr,"bhEne_SetWeponAtr(en12)"); w->sv->weapon_atr(w,5,21,4.0f);
    NEED(w,check_player,"bhCheckPlayer(en12)"); w->sv->check_player(w);
    if (w->flg&0x10u) check_wall(w);
    look(w); fire_hand(w,3);
    NEED(w,calc_model,"bhCalcModel(en12)"); w->sv->calc_model(w);
    control_player(w);
}

/* en12.c:2089 bhEne12_SetFireBintaEffect(epw, action). Ausser von EN12 selbst
   ruft ihn der Effektcontroller bhEff_E12_BintaEffControl (effsub2.c:3658,
   bhJumpEffect2[26] = Effekt 276) mit action 0/1/2 (effsub2.c:3678-3688). */
void recvx_en12_set_fire_binta_effect(RECVX_ENEMY_WORK* w, int action)
{
    if (!w) return;
    if (!w->exp0) { missing(w,"bhEne12_SetFireBintaEffect: exp0 fehlt"); return; }
    fire_hand(w,action);
}
