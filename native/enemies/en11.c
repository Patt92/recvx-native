/* ID 11 (eneset.c:51), eigenstaendiger nativer Mechanismus.
   Semantikreferenz en11.c:53..1248. Die zehn Move-Faelle sind unten in
   scan/turn/restore zerlegt, ohne ihre Reihenfolge oder Timer zu aendern.
   Keine Originalkoerper, Generatoren, globale Spielwork oder Raumregeln. */
#include "en11.h"
#include "en03_collision.h"
#include "ninja_matrix.h"
#include <math.h>
#include <string.h>

static const RECVX_EN11_SERVICES* extra;
void recvx_en11_bind(const RECVX_EN11_SERVICES* s) { extra = s; }
static int gap(RECVX_ENEMY_WORK* w, const char* name)
{
    if (w->sv && w->sv->gap) w->sv->gap(name);
    return 0;
}
static RECVX_EN11_STATE* state(RECVX_ENEMY_WORK* w) { return (RECVX_EN11_STATE*)w->exp0; }
/* zonzon1.c:864, bewusst asin/Quadrantenkorrektur statt atan2-Ersatz. */
static int arc(float y, float x)
{
    float length = sqrtf(x*x + y*y);
    if (length == 0.0f) return 0;
    int a = (int)(10430.381f * asinf(y/length));
    return x < 0.0f ? (int16_t)(32768-a) : a;
}
static RECVX_ENEMY_VEC position(const RECVX_ENEMY_WORK* w)
{
    RECVX_ENEMY_VEC p = {w->px,w->py,w->pz}; return p;
}
static int player(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* p)
{
    if (!w->sv || !w->sv->player_get || !w->sv->player_get(p))
        return gap(w,"bhEne11: plp");
    return 1;
}
static int effect_read(RECVX_ENEMY_WORK* w, RECVX_EN11_EFFECT* e)
{
    if (!extra || !extra->effect_get || state(w)->effect_slot < 0 ||
        !extra->effect_get(state(w)->effect_slot,e))
        return gap(w,"bhEne11: linked bhEff261 work");
    return 1;
}
/* en11.c:1115 bhEne11_LightControl. Auch von aussen erreichbar: der
   Ereignisbefehl Sub_controll 148 ruft bhEne11_LightControl(work, v0)
   direkt (event.c:9912-9923). */
static int light(RECVX_ENEMY_WORK* w, int mode)
{
    RECVX_EN11_EFFECT e;
    if (!extra || !extra->effect_set) return gap(w,"bhEne11_LightControl");
    if (!state(w)) return gap(w,"bhEne11_LightControl: exp0 fehlt");
    if (!effect_read(w,&e)) return 0;
    switch (mode) {
    case 0: e.mode0=1; e.axp=0x38e; break;
    case 1: e.mode0=3; break;
    case 2: e.mode0=0; break;
    case 3: e.mode0=2; e.ax=e.axp=0x38e; break;
    default: break; /* en11.c:1117-1137: kein default-Zweig, nichts geschrieben. */
    }
    extra->effect_set(state(w)->effect_slot,&e); return 1;
}
int recvx_en11_light_control(RECVX_ENEMY_WORK* w, int mode)
{
    if (!w) return 0;
    return light(w,mode);
}
static int line(RECVX_ENEMY_WORK* w, const RECVX_ENEMY_VEC* a,
                RECVX_ENEMY_VEC* b, RECVX_ENEMY_ATR* hit, uint32_t* id)
{
    RECVX_ENEMY_VEC normal;
    /* hitchkl.c:34: variant 1, originale Flag-/Floorparameter. */
    return w->sv->line(1,a,b,&normal,0,0,hit,id);
}
static void near_wall(RECVX_ENEMY_WORK* w)
{
    RECVX_EN11_STATE* e=state(w);
    RECVX_ENEMY_VEC a=position(w), b={a.x-999.0f*e->basis[4],
        a.y-999.0f*e->basis[5],a.z-999.0f*e->basis[6]};
    /* en11.c:1104: EXP0 0x58 = Trefferflaeche oder NULL. Die Gueltigkeit
       haengt am Treffer selbst, nicht an der optionalen Flaechenidentitaet
       des Anbieters (0 = "keine Identitaet" ist dort ein gueltiger Treffer). */
    e->wall_id=0;
    e->wall_valid=(uint8_t)(line(w,&a,&b,&e->wall,&e->wall_id)!=0);
    if (e->wall_valid) {
        w->px=w->pxb=b.x; w->py=w->pyb=b.y; w->pz=w->pzb=b.z;
    }
}
static int initialize(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* s=w->sv;
    if (!s || !s->calloc_work || !s->line || !s->system_flags_or ||
        !extra || !extra->flags_clear || !extra->effect_create || !extra->effect_set)
        return gap(w,"bhEne11_Init: allocation/line/flags/effect services");
    if (!w->exp0) {
        w->exp0=s->calloc_work((uint32_t)sizeof(RECVX_EN11_STATE));
        if (!w->exp0) return gap(w,"bhEne_CallocWork: EN11");
        state(w)->effect_slot=-1;
    }
    RECVX_EN11_STATE* e=state(w);
    extra->flags_clear(1,1);
    w->flg=(w->flg|0x1848u)&~6u;
    w->ar=3.5f; w->ah=1.5f; w->car=w->cah=1.0f; w->spd=0.2f;
    w->mode0=w->mode1=1; w->mode2=w->mode3=0;
    njUnitMatrix((NJS_MATRIX*)&e->basis);
    if (!w->type) {
        njRotateY((NJS_MATRIX*)&e->basis,w->ay);
        njRotateZ((NJS_MATRIX*)&e->basis,0x8000);
    } else {
        njRotateY((NJS_MATRIX*)&e->basis,(w->type-1)*0x4000);
        njRotateX((NJS_MATRIX*)&e->basis,0x4000);
        njRotateY((NJS_MATRIX*)&e->basis,w->ay);
    }
    e->alarm=0;
    if (e->effect_slot < 0) {
        const RECVX_EN11_EFFECT_TEMPLATE t={0x105,1,0,0,{0,0,0},{0,0,0},0};
        e->effect_slot=extra->effect_create(&t,w);
        if (e->effect_slot < 0) return gap(w,"bhSetEffectTb(261): pool exhausted; original dereferences absent effect");
        RECVX_EN11_EFFECT f;
        if (!effect_read(w,&f)) return 0;
        f.mode0=0; extra->effect_set(e->effect_slot,&f);
    } else {
        RECVX_EN11_EFFECT f;
        if (!effect_read(w,&f)) return 0;
        f.mode0=0; f.ct2=0; extra->effect_set(e->effect_slot,&f);
    }
    w->clp_jno[0]=1; w->clp_jno[1]=4; w->clp_jno[2]=-1;
    near_wall(w); w->flg|=0x80000u; e->initialized=1; return 1;
}
static int aim(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC target, int count)
{
    if (!extra || !extra->node_angles_get || !extra->node_angles_set)
        return gap(w,"bhEne11_CameraSet: objP angles");
    NJS_MATRIX inverse; memcpy(inverse,state(w)->basis,sizeof inverse);
    njInvertMatrix(&inverse);
    NJS_VECTOR in={target.x,target.y,target.z}, v;
    njCalcVector(&inverse,&in,&v); njUnitVector(&v);
    int32_t a[3],b[3];
    if (!extra->node_angles_get(w,2,a) || !extra->node_angles_get(w,3,b))
        return gap(w,"bhEne11_CameraSet: objP[2/3]");
    if (count) {
        /* en11.c:1053-1068. njRotateEx(ang, 0) dreht Z, dann Y, dann X
           (ps2_NaMatrix.c:1759-1764); erst objP[2], dann objP[3]. */
        NJS_MATRIX rotation; njUnitMatrix(&rotation);
        njRotateZ(&rotation,a[2]); njRotateY(&rotation,a[1]); njRotateX(&rotation,a[0]);
        njRotateZ(&rotation,b[2]); njRotateY(&rotation,b[1]); njRotateX(&rotation,b[0]);
        NJS_VECTOR old={0,0,-1}, axis;
        njCalcVector(&rotation,&old,&old);
        float cross=njOuterProduct(&old,&v,&axis);
        njUnitVector(&axis);
        /* asinf ausserhalb [-1,1] (Rundung des Kreuzprodukts) liefert kein
           belegtes Originalergebnis; Luecke statt Ersatzwinkel. */
        if (!(cross>=0.0f) || cross>1.0f)
            return gap(w,"bhEne11_CameraSet: asinf(|vb x v|) ausserhalb [0,1]");
        int turn=(int)(10430.381f*asinf(cross))/count;
        /* njRotate (ps2_NaMatrix.c:737) baut den Quaternionvektorteil als
           Achse*rsqrt(|Achse|^2)*sin(ang/2). Bei turn == 0 ist sin 0 und die
           VU-Multiplikation 0*Q liefert 0 auch fuer die Nullachse (|vb x v|
           = 0): die Drehung ist exakt die Einheit, v = vb. Der native
           njRotate wuerde fuer die Nullachse 0*inf rechnen; darum wird genau
           dieser Fall ohne Drehung gefuehrt — dasselbe Ergebnis, kein
           Ersatzwert. */
        if (turn==0) { v=old; }
        else { njUnitMatrix(&rotation); njRotate(&rotation,&axis,turn); njCalcVector(&rotation,&old,&v); }
    }
    a[1]=arc(-v.x,-v.z);
    b[0]=arc(v.y,sqrtf(v.x*v.x+v.z*v.z)); if (b[0]<0) b[0]=0;
    extra->node_angles_set(w,2,a); extra->node_angles_set(w,3,b); return 1;
}
static int surface_vector(RECVX_ENEMY_WORK* w, RECVX_ENEMY_VEC* v)
{
    switch (w->type) {
    case 0: { int a=arc(-state(w)->basis[10],-state(w)->basis[8]);
        v->x=8.0f*njCos(a); v->z=8.0f*njSin(a); break; }
    case 1: v->x=0; v->z=16; break;
    case 2: v->x=16; v->z=0; break;
    case 3: v->x=0; v->z=-16; break;
    case 4: v->x=-16; v->z=0; break;
    default: return gap(w,"bhEne11: unsupported type leaves original vector undefined");
    }
    RECVX_ENEMY_PLAYER p;
    if (!player(w,&p)) return 0;
    v->y=-fabsf(p.pos.y-w->py); return 1;
}
static void forward(RECVX_ENEMY_WORK* w)
{
    const float* m=state(w)->basis;
    w->px-=m[8]*w->spd; w->py-=m[9]*w->spd; w->pz-=m[10]*w->spd;
}
static int sound(RECVX_ENEMY_WORK* w,int se)
{
    if (!w->sv->enemy_se || !w->sv->enemy_now) return gap(w,"bhEne_CallSE/ENOW");
    if (!(w->flg&0x10000u)) { RECVX_ENEMY_VEC p=position(w); w->sv->enemy_se(w->sv->enemy_now(),&p,se); }
    return 1;
}
static void hum(RECVX_ENEMY_WORK* w)
{
    if (!w->sv->enemy_se_playing || !w->sv->enemy_now) { gap(w,"ChechPlayEnemySe"); return; }
    uint32_t index=w->sv->enemy_now();
    if (!w->sv->enemy_se_playing(index,0x11300)) sound(w,0x11300);
    else if (extra && extra->se_pan) { RECVX_ENEMY_VEC p=position(w); extra->se_pan(index,&p,0x11300); }
    else gap(w,"bhEne_SetSEPan");
}
static int select_dir(RECVX_ENEMY_WORK* w, int* out)
{
    const float* m=state(w)->basis;
    if (!w->sv->line || !w->sv->ground || !w->sv->random_next)
        return gap(w,"bhEne11_SelectDir: line/ground/rand");
    const RECVX_ENEMY_VEC dir[4]={{-m[8],-m[9],-m[10]},
        {m[0],m[1],m[2]},{m[8],m[9],m[10]},{-m[0],-m[1],-m[2]}};
    RECVX_ENEMY_VEC origin={w->px+m[4],w->py+m[5],w->pz+m[6]};
    int available[4]={0,0,0,0};
    for (int i=0;i<4;i++) {
        RECVX_ENEMY_VEC p={origin.x+20.0f*dir[i].x,origin.y+20.0f*dir[i].y,origin.z+20.0f*dir[i].z};
        if (!line(w,&origin,&p,NULL,NULL)) {
            RECVX_ENEMY_VEC end={p.x-2*m[4],p.y-2*m[5],p.z-2*m[6]};
            if (line(w,&p,&end,NULL,NULL) && p.y-w->sv->ground(&p)>10.0f) available[i]=1;
        }
    }
    int choice;
    if (!w->type && (float)w->sv->random_next()/2147483648.0f>0.5f) {
        RECVX_ENEMY_PLAYER p; if (!player(w,&p)) return 0;
        NJS_MATRIX inv; memcpy(inv,m,sizeof inv); njInvertMatrix(&inv);
        NJS_POINT3 in={p.pos.x,p.pos.y,p.pos.z},local; njCalcPoint(&inv,&in,&local);
        /* ELF 0x1d55e0 andi 0xff, NOT 0xffff. Preserve unusual original mask. */
        choice=((arc(-local.x,-local.z)&0xff)+0x2000)/0x4000;
    } else {
        choice=(int)(4.0f*((float)w->sv->random_next()/2147483648.0f));
        if (choice<0 || choice>3) return gap(w,"bhEne11_SelectDir: rand outside original index range");
    }
    for (int i=0;i<4 && !available[choice];i++) choice=(choice+1)&3;
    *out=choice; return 1;
}
/* en11.c:303/455/534/632: identical phase ordering, distinct target formulae. */
static int scan(RECVX_ENEMY_WORK* w, int mode)
{
    RECVX_EN11_STATE* e=state(w);
    RECVX_ENEMY_PLAYER p; if (!player(w,&p)) return 0;
    if (!w->mode3) {
        w->ct0=99; w->ct1=mode==5?25:8;
        w->ct2=mode==4?e->dir*25:(mode==6?25:0); w->ct3=0;
        if (!light(w,0)) return 0;
        w->spd=0.2f;
        if (mode==1) w->ay=arc(-e->basis[10],-e->basis[8]);
        w->mode3=1;
    }
    if (w->mode3==1) {
        RECVX_ENEMY_VEC v; v.y=-fabsf(p.pos.y-w->py);
        if (mode==1) { int a=w->ay+w->ct2*0x28f; v.x=8*njCos(a); v.z=8*njSin(a); }
        else if (mode==4 || mode==5) {
            float f=16.0f+16.0f*njSin(w->ct2*0x28f);
            v.x=(mode==4?-e->basis[8]:e->basis[4])*f;
            v.z=(mode==4?-e->basis[10]:e->basis[6])*f;
        } else {
            int a=(int)(16384.0f+16384.0f*njCos(w->ct2*0x28f));
            switch (w->type) {
            case 1: v.x=16*njCos(a); v.z=16*njSin(a); break;
            case 2: v.x=16*njSin(a); v.z=16*njCos(a); break;
            case 3: v.x=16*njCos(a); v.z=-16*njSin(a); break;
            case 4: v.x=-16*njSin(a); v.z=16*njCos(a); break;
            default: return gap(w,"bhEne11_MV06: undefined original type");
            }
        }
        ++w->ct2; if (!aim(w,v,w->ct1)) return 0;
        if (w->ct1) --w->ct1;
        forward(w);
        if (w->ct0--==0) {
            if (!select_dir(w,&e->dir)) return 0;
            if (e->dir) {
                if (!light(w,1)) return 0;
                w->mode1=1; w->mode2=3; w->mode3=0;
            } else w->ct0=99;
        }
        hum(w);
    }
    if (e->alarm) { w->mode1=1; w->mode2=7; w->mode3=0; }
    return 1;
}
static int restore(RECVX_ENEMY_WORK* w)
{
    const RECVX_ENEMY_SERVICES* s=w->sv;
    if (!s->joint_matrix || !s->set_motion || !extra || !extra->node_work_flags)
        return gap(w,"bhEne11_MV09: matrices/O_WORK flags/motion");
    float joint[16];
    if (!s->joint_matrix(w,1,state(w)->basis) || !s->joint_matrix(w,0,joint))
        return gap(w,"bhEne11_MV09: owP[0/1]");
    state(w)->basis[12]=state(w)->basis[13]=state(w)->basis[14]=0;
    w->px=joint[12]+3*state(w)->basis[4]; w->py=joint[13]+3*state(w)->basis[5];
    w->pz=joint[14]+3*state(w)->basis[6];
    extra->node_work_flags(w,2,2,0); extra->node_work_flags(w,3,2,0);
    w->mtn_room=0; w->mtn_no=0; w->mtn_add=w->frm_no=0;
    s->set_motion(w,w->mtn_add,w->mtn_md);
    extra->node_work_flags(w,2,0,2); extra->node_work_flags(w,3,0,2);
    near_wall(w);
    w->mode0=w->mode1=w->mode3=1; w->mode2=w->type?6:4;
    w->ct0=150; w->ct1=32; w->ct2=0;
    w->flg&=~0x80000u; w->spd=0.2f; forward(w); return 1;
}
static int move(RECVX_ENEMY_WORK* w)
{
    RECVX_EN11_STATE* e=state(w);
    if (w->mode1==1) {
        RECVX_EN11_EFFECT f;
        if (!effect_read(w,&f) || !w->sv->system_flags || !w->sv->system_flags_or)
            return gap(w,"bhEne11_BR00: effect/system flags");
        if (f.ct2) e->alarm=1;
        if (e->alarm) {
            if (!(w->sv->system_flags(2)&0x2000u)) w->sv->system_flags_or(1,1);
        } else if ((w->sv->system_flags(1)&1) && f.mode0==2) {
            e->alarm=1; f.ct2=1;
            if (!extra->effect_set) return gap(w,"bhEne11_BR00: effect_set");
            extra->effect_set(e->effect_slot,&f);
        }
    }
    switch (w->mode2) {
    case 0:
        if (!w->mode3) { w->ct0=30; w->mode3=1; }
        if (w->mode3==1 && w->ct0--==0) { w->mode1=1; w->mode2=w->type?6:1; w->mode3=0; }
        break;
    case 1: case 4: case 5: case 6: return scan(w,w->mode2);
    case 2:
        if (!w->mode3) { w->ct0=99; ++w->mode3; w->spd=0.2f; }
        if (w->mode3==1) {
            forward(w);
            if (w->ct0--==0) { w->mode1=1; w->mode3=0; w->mode2=w->type?6:1; }
            hum(w);
        }
        break;
    case 3:
        if (!w->mode3) {
            const int16_t angle[4]={0,-0x222,0x38e,0x222};
            const int time[4]={0,29,35,29};
            if (e->dir<0 || e->dir>3) return gap(w,"bhEne11_MV03: invalid direction");
            w->way=angle[e->dir]; w->ct0=time[e->dir];
            if (e->dir) sound(w,0x12301);
            w->ct1=16; ++w->mode3;
        }
        if (w->mode3==1) {
            njRotateY((NJS_MATRIX*)&e->basis,w->way);
            if (w->ct0--==0) { w->mode1=1; w->mode3=0; w->mode2=w->type?6:1; }
            if (w->type) {
                RECVX_ENEMY_VEC v; if (!surface_vector(w,&v) || !aim(w,v,w->ct1)) return 0;
                if (w->ct1) --w->ct1;
            }
        }
        break;
    case 7: if (!w->mode3) { sound(w,0x2302); ++w->mode3; } break;
    case 8: if (!w->mode3) { w->mode1=0; if (!light(w,1)) return 0; ++w->mode3; } break;
    case 9: return restore(w);
    default: return gap(w,"bhEne11_MoveMode2: invalid mode");
    }
    return 1;
}
/* en11.c:865..1020; four probes ordered --,-+,+-,++. */
static int edge(RECVX_ENEMY_WORK* w)
{
    RECVX_EN11_STATE* e=state(w);
    if (!e->wall_valid) return 1;             /* en11.c:854 EXP0_I(0x58) != 0 */
    float* m=e->basis;
    int normal_axis,axis0,axis1; float sign;
    if (m[5]>=0.9f) { normal_axis=1; sign=-1; axis0=0; axis1=2; }
    else if (m[5]<=-0.9f) { normal_axis=1; sign=1; axis0=0; axis1=2; }
    else if (m[4]>=0.9f) { normal_axis=0; sign=-1; axis0=1; axis1=2; }
    else if (m[4]<=-0.9f) { normal_axis=0; sign=1; axis0=1; axis1=2; }
    else if (m[6]>=0.9f) { normal_axis=2; sign=-1; axis0=0; axis1=1; }
    else if (m[6]<=-0.9f) { normal_axis=2; sign=1; axis0=0; axis1=1; }
    else return gap(w,"bhEne03_GetWallDir: undefined non-axis original result");
    float radius=1+w->ar;
    float center[3]={w->px,w->py,w->pz};
    for (int i=0;i<4;i++) {
        float p[3]={center[0],center[1],center[2]},q[3];
        p[axis0]+=(i<2?-radius:radius); p[axis1]+=(i%2?radius:-radius);
        memcpy(q,p,sizeof p); q[normal_axis]+=sign*radius;
        RECVX_ENEMY_VEC from={p[0],p[1],p[2]},to={q[0],q[1],q[2]};
        if (!line(w,&from,&to,NULL,NULL)) {
            from.x=w->px+m[4]; from.y=w->py+m[5]; from.z=w->pz+m[6];
            to.x=from.x-999*m[4]; to.y=from.y-999*m[5]; to.z=from.z-999*m[6];
            /* en11.c:1013 bhEne03_GetWall (en03.c:5203), dann
               bhEne03_CollisionBoxEdge (en03.c:5938) aus dem gemeinsamen
               nativen EN03-Kollisionssatz. */
            e->wall_id=0;
            e->wall_valid=(uint8_t)(line(w,&from,&to,&e->wall,&e->wall_id)!=0);
            if (e->wall_valid) {
                RECVX_T3_ATR hp;
                memset(&hp,0,sizeof hp);
                hp.flg=(uint8_t)e->wall.flg; hp.type=(uint8_t)e->wall.type; hp.id=e->wall.id;
                hp.flr_no=(int8_t)e->wall.flr_no; hp.attr=e->wall.attr;
                hp.px=e->wall.px; hp.py=e->wall.py; hp.pz=e->wall.pz;
                hp.w=e->wall.w; hp.h=e->wall.h; hp.d=e->wall.d;
                hp.prm0=e->wall.prm0; hp.prm1=e->wall.prm1; hp.prm2=e->wall.prm2; hp.prm3=e->wall.prm3;
                RECVX_ENEMY_VEC pos=position(w);
                (void)recvx_en03_collision_box_edge(&hp,&pos,w->ar);
                w->px=pos.x; w->py=pos.y; w->pz=pos.z; break;
            }
        }
    }
    return 1;
}
static int collide(RECVX_ENEMY_WORK* w)
{
    if (!w->sv->collision_works || !w->sv->line) return gap(w,"bhEne11_CollisionWalls: EN03/line");
    const float* m=state(w)->basis;
    RECVX_ENEMY_VEC offset={3.5f*m[4],3.5f*m[5],3.5f*m[6]};
    w->px+=offset.x; w->py+=offset.y; w->pz+=offset.z;
    w->sv->collision_works(w);
    int result=edge(w);
    w->px-=offset.x; w->py-=offset.y; w->pz-=offset.z;
    return result;
}
void recvx_en11(RECVX_ENEMY_WORK* w)
{
    if (!w || !w->sv) return;
    if (!w->sv->set_motion || !w->sv->calc_model) { gap(w,"bhEne11: motion/model"); return; }
    if (w->mode0==0) { if (!initialize(w)) return; }
    else {
        if (!state(w) || !state(w)->initialized) { gap(w,"bhEne11: missing initialized exp0"); return; }
        switch (w->mode0) {
        case 1: if (!move(w)) return; break;
        /* en11.c:826/832/838 Nage/Damage/Die are proven empty originals. */
        case 2: case 3: case 4: break;
        case 5: if (!w->sv->event) { gap(w,"bhEne_Event"); return; } w->sv->event(w); break;
        default: gap(w,"bhEne11_Mode0: invalid mode"); return;
        }
    }
    if (w->mtn_room) {
        w->ax=w->ay=w->az=0;
        uint32_t saved=w->flg; w->flg&=~0x1000u;
        w->sv->set_motion(w,w->mtn_add,w->mtn_md); w->sv->calc_model(w);
        w->flg=saved|0x80000u; return;
    }
    if (w->flg&0x80000u) {
        w->mtn_no=0; w->mtn_add=w->frm_no=0; w->sv->set_motion(w,w->mtn_add,w->mtn_md);
        w->flg&=~0x80000u;
        RECVX_ENEMY_VEC v; if (!surface_vector(w,&v) || !aim(w,v,0)) return;
    }
    if (!collide(w)) return;
    njUnitMatrix((NJS_MATRIX*)&w->root_mtx);
    njTranslate((NJS_MATRIX*)&w->root_mtx,w->px,w->py,w->pz);
    njMultiMatrix((NJS_MATRIX*)&w->root_mtx,(NJS_MATRIX*)&state(w)->basis);
    w->sv->calc_model(w);
}
