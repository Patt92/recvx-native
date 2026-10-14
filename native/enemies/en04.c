/* C04 — eigenstaendige Cerberus-Zustandsmaschine auf WorldEntity-Work.
   Semantikreferenz ausschliesslich src/ps2/veronica/prog/en04.c und zonzon.c.
   Keine Originalkoerper/Generatoren, keine zweite Spieler-/Gegnerwelt.
   Dienste lesen/schreiben die bestehenden Besitzer. Nicht vorhandene Dienste
   brechen den betreffenden Mechanismus mit benannter Luecke ab. */
#include "en04.h"
#include "en04_tables.h"
#include "en03_lib.h"
#include "../ninja_matrix.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define OFF(f,n) _Static_assert(offsetof(RECVX_EN04_EXP0,f)==n,"EN04 exp0 " #f)
OFF(search,0); OFF(dmg_sub,3); OFF(stair_flr,4); OFF(stair_h,5);
OFF(wall_hist,6); OFF(flg,0x10); OFF(ct_14,0x14); OFF(ct_18,0x18);
OFF(neck,0x1c); OFF(dist,0x20); OFF(target,0x24); OFF(link,0x30);
OFF(wall_hit,0x3c); OFF(stair_atr,0x40); OFF(line_hit,0x44);
OFF(u48,0x48); OFF(esc_ct,0x4c); OFF(esc_turn,0x50); OFF(reserved_54,0x54);
OFF(stair_rec,88);
#undef OFF
static RECVX_EN04_CONTEXT context;
static RECVX_EN04_DATA data;
static uint32_t contact_flags, area_flags; /* en04.c:411 */
void recvx_en04_set_context(const RECVX_EN04_CONTEXT* c) {
    if(c) context=*c; else memset(&context,0,sizeof(context));
}
void recvx_en04_set_data(const RECVX_EN04_DATA* d) {
    if(d) data=*d; else memset(&data,0,sizeof(data));
}
void recvx_en04_reset_shared(void) { contact_flags=area_flags=0; }
uint32_t recvx_en04_contact_flags(void) { return contact_flags; }
uint32_t recvx_en04_area_flags(void) { return area_flags; }
static RECVX_EN04_EXP0* ex(RECVX_ENEMY_WORK* w) { return w->exp0; }
static void gap(const RECVX_ENEMY_WORK* w,const char* n) {
    if(w && w->sv && w->sv->gap) w->sv->gap(n);
}
#define NEED(member,name) do { if(!w->sv || !w->sv->member) {gap(w,name);return;} } while(0)
#define XNEED(member,name) do { if(!context.en04 || !context.en04->member) {gap(w,name);return;} } while(0)
#define NEEDI(member,name) do { if(!w->sv || !w->sv->member) {gap(w,name);return 0;} } while(0)
#define XNEEDI(member,name) do { if(!context.en04 || !context.en04->member) {gap(w,name);return 0;} } while(0)
static int player(RECVX_ENEMY_WORK* w,RECVX_ENEMY_PLAYER* p) {
    if(w->sv && w->sv->player_get && w->sv->player_get(p)) return 1;
    gap(w,"plp (EN04)"); return 0;
}
static int put_player(RECVX_ENEMY_WORK* w,const RECVX_ENEMY_PLAYER* p) {
    if(w->sv && w->sv->player_set) {w->sv->player_set(p);return 1;}
    gap(w,"plp write (EN04)");return 0;
}
static RECVX_ENEMY_VEC position(const RECVX_ENEMY_WORK* w) {
    RECVX_ENEMY_VEC p={w->px,w->py,w->pz};return p;
}
static void transform(RECVX_ENEMY_VEC origin,int ax,int ay,int az,
                       RECVX_ENEMY_VEC local,RECVX_ENEMY_VEC* out) {
    NJS_MATRIX m; NJS_POINT3 a={local.x,local.y,local.z},b;
    njUnitMatrix(&m);njTranslate(&m,origin.x,origin.y,origin.z);
    njRotateXYZ(&m,ax,ay,az);njCalcPoint(&m,&a,&b);
    out->x=b.x;out->y=b.y;out->z=b.z;
}
static void advance(RECVX_ENEMY_WORK* w,int relative) { /* pwksub.c:133 */
    int a=(w->ay+relative)&65535;
    w->px-=w->spd*njSin(a);w->pz-=w->spd*njCos(a);
}
static int random_value(RECVX_ENEMY_WORK* w,int* out) {
    if(!w->sv || !w->sv->random_next) {gap(w,"rand");return 0;}
    *out=w->sv->random_next();return 1;
}
static int last_frame(RECVX_ENEMY_WORK* w) {
    if(!w->sv || !w->sv->motion_frames) {gap(w,"mnwP[mtn].frm_num");return 0;}
    uint32_t n=w->sv->motion_frames(w,w->mtn_no);
    if(!n) {gap(w,"mnwP[mtn] (EN04)");return 0;}
    return w->frm_no/65536==(int32_t)n-1;
}
static int joint(RECVX_ENEMY_WORK* w,int n,RECVX_ENEMY_VEC* p) {
    float m[16];
    if(w->sv && w->sv->joint_matrix && w->sv->joint_matrix(w,n,m)) {
        p->x=m[12];p->y=m[13];p->z=m[14];return 1;
    } gap(w,"mlwP->owP[n].mtx (EN04)");return 0;
}
static int ply_joint(RECVX_ENEMY_WORK* w,int n,RECVX_ENEMY_VEC* p) {
    float m[16];
    if(w->sv && w->sv->player_joint_matrix && w->sv->player_joint_matrix(n,m)) {
        p->x=m[12];p->y=m[13];p->z=m[14];return 1;
    } gap(w,"plp->mlwP->owP[n].mtx (EN04)");return 0;
}
static int parts(RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC* p) { /* zonzon.c:286 */
    NJS_MATRIX m; RECVX_ENEMY_VEC local;int32_t ang[3];
    XNEEDI(node_transform,"mlwP->objP[n] (CalcPartsPos)");
    njUnitMatrix(&m);njTranslate(&m,w->px,w->py,w->pz);njRotateXYZ(&m,w->ax,w->ay,w->az);
    for(unsigned i=0;i<6;i++) {
        if(!context.en04->node_transform(w,recvx_en04_tree[i],&local,ang)) {
            gap(w,"mlwP->objP[n] (CalcPartsPos)");return 0;
        }
        njTranslate(&m,local.x,local.y,local.z);njRotateXYZ(&m,ang[0],ang[1],ang[2]);
    }
    p->x=m[12];p->y=m[13];p->z=m[14];return 1;
}
static void node_pos(RECVX_ENEMY_WORK* w,int axis,float v) {
    if(context.en04 && context.en04->node_pos_set) context.en04->node_pos_set(w,0,axis,v);
    else gap(w,"mlwP->objP[0].pos");
}
static void root_yaw_zero(RECVX_ENEMY_WORK* w) {
    if(context.en04 && context.en04->node_ang_set) context.en04->node_ang_set(w,0,1,0);
    else gap(w,"mlwP->objP[0].ang");
}
static void speed_up(RECVX_ENEMY_WORK* w,float g,float limit) { /* :3631 */
    if(w->spd<limit) {w->spd+=g;if(w->spd>limit)w->spd=limit;}
}
static void speed_down(RECVX_ENEMY_WORK* w,float g,float limit) { /* :3644 */
    if(w->spd>limit) {w->spd-=g;if(w->spd<limit)w->spd=limit;}
}
static void run_motion(RECVX_ENEMY_WORK* w,int rot) { /* :3657 */
    const uint32_t normal[3]={3,1,2}, fence[3]={41,34,40};
    unsigned idx=rot==0?1:rot>0?0:2;
    uint32_t n=(ex(w)->flg&0x8000)?fence[idx]:normal[idx];
    if(n==w->mtn_no) return;
    w->mtn_no=n;
    if(w->hokan_count<8) {w->hokan_count=7;w->hokan_rate=0;w->mtn_md=32;}
}
int recvx_en04_chg_mtn(RECVX_ENEMY_WORK* w,uint32_t n,int32_t f,int32_t rate) { /* :3703 */
    w->mtn_add=65536;
    if(w->mtn_no==n)return -1;
    w->mtn_no=n;w->frm_no=f;w->hokan_count=rate;w->hokan_rate=32768;w->mtn_md=0;
    w->flg&=~0x2040000u;return 0;
}
static int line_check_normal(RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC from,
                             RECVX_ENEMY_VEC* to,RECVX_ENEMY_ATR* rec,
                             RECVX_ENEMY_VEC* normal,int* hit) {
    if(!w->sv || !w->sv->line) {gap(w,"bhCollisionCheckLine");return 0;}
    uint32_t id;
    *hit=w->sv->line(1,&from,to,normal,0,-1,rec,&id);return 1;
}
static int line_check(RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC from,
                       RECVX_ENEMY_VEC* to,RECVX_ENEMY_ATR* rec,int* hit) {
    RECVX_ENEMY_VEC normal;
    return line_check_normal(w,from,to,rec,&normal,hit);
}
static void clr_use_kaidan_flag(RECVX_ENEMY_WORK* w) {
    if(w->sv && w->sv->stair_use) w->sv->stair_use(ex(w)->stair_idx,0);
    else gap(w,"bhClrUseKaidanFlag");
}
static int check_floor_num(RECVX_ENEMY_WORK* w,float py) {
    if(w->sv && w->sv->floor_num) return w->sv->floor_num(py);
    gap(w,"bhCheckFloorNum");
    return (int)w->flr_no;
}
static int lever_check(RECVX_ENEMY_WORK* w) {
    if(w->sv && w->sv->lever_check) return w->sv->lever_check();
    gap(w,"bhEne_LeverCheck");
    return 0;
}
static int dir_wall(RECVX_ENEMY_WORK* w,int ang,float step,int* hit) {
    if(!w->sv || !w->sv->wall_type) {gap(w,"bhCheckWallType");return 0;}
    RECVX_ENEMY_VEC p=position(w);int a=(w->ay+ang)&65535;
    p.x-=step*njSin(a);p.z-=step*njCos(a);
    *hit=w->sv->wall_type(&p,w->flg,w->ar,w->ah);return 1;
}
static int side_wall(RECVX_ENEMY_WORK* w,float step,int both,int* hit) {
    int l,r;if(!dir_wall(w,-16384,step,&l)||!dir_wall(w,16384,step,&r))return 0;
    *hit=l&&r?both:(l?1:0)-(r?1:0);return 1;
}
static int area(RECVX_ENEMY_WORK* w,RECVX_ENEMY_VEC p,int floor,int type,
                 RECVX_ENEMY_ATR* out,int* index) { /* zonzon.c:373 */
    XNEEDI(flr_count,"rom->flrp/sys->mflrp");XNEEDI(flr_at,"rom->flrp/sys->mflrp");
    for(uint32_t i=0;i<context.en04->flr_count();i++) {
        RECVX_ENEMY_ATR a;
        if(!context.en04->flr_at(i,&a)) {gap(w,"flrp[index]");return -1;}
        if((a.flg&1)&&a.type==2&&a.flr_no==floor&&a.prm0==w->id&&a.prm1==type&&
           recvx_t3_pos_check(p.x,p.z,a.px,a.pz,a.w,a.d)) {
            if(out)*out=a;if(index)*index=(int)i;return 1;
        }
    }return 0;
}
static int area_tag(RECVX_ENEMY_WORK* w,int type,int tag,RECVX_ENEMY_ATR* out) {
    XNEEDI(flr_count,"rom->flrp/sys->mflrp");XNEEDI(flr_at,"rom->flrp/sys->mflrp");
    for(uint32_t i=0;i<context.en04->flr_count();i++) {
        RECVX_ENEMY_ATR a;
        if(!context.en04->flr_at(i,&a)) {gap(w,"flrp[index]");return -1;}
        if((a.flg&1)&&a.type==2&&a.flr_no==w->flr_no&&a.prm0==w->id&&a.prm1==type&&a.prm3==tag)
            {*out=a;return 1;}
    }return 0;
}
static int grand(RECVX_ENEMY_WORK* w,float* out) {
    if(!w->sv || !w->sv->grand) {gap(w,"rom->grand");return 0;}
    *out=w->sv->grand((int)w->flr_no+2);return 1;
}
/* Kein gemeinsamer Landehelfer: en04.c schreibt den Bodenabgleich in jedem
   Sprungzweig (MV07 :2369/:2473, MV08 :2449/:2538, DG00 :3170, DG01 :3300)
   mit eigener Flag- und Reihenfolgewahl selbst. */
static void vibration(RECVX_ENEMY_WORK* w,int no) {
    if(no<0||no>2)return; /* zonzon.c:1346: kein default */
    if(w->sv&&w->sv->vibration)w->sv->vibration(1,9+no);
    else gap(w,"bhEne_SetVibration/StartVibrationEx");
}
static void ply_voice(RECVX_ENEMY_WORK* w,int no) {
    if(w->flg&0x10000)return;
    if(w->sv&&w->sv->player_voice)w->sv->player_voice(no);else gap(w,"CallPlayerVoice");
}
void recvx_en04_se_play(RECVX_ENEMY_WORK* w,int32_t no) { /* :4048 */
    if(w->flg&0x10000)return;
    NEED(enemy_se,"RequestEnemySe");NEED(enemy_now,"sys->enow");
    RECVX_ENEMY_VEC p=position(w);w->sv->enemy_se(w->sv->enemy_now(),&p,no);
}
static void mode(RECVX_ENEMY_WORK* w,int m0,int m1,int m2,int m3) {
    w->mode0=m0;w->mode1=m1;w->mode2=m2;w->mode3=m3;
}
static void chase(RECVX_ENEMY_WORK* w) {mode(w,1,1,2,0);}
static void player_mode(RECVX_ENEMY_PLAYER* p,int a,int b,int c,int d) {
    p->mode0=a;p->mode1=b;p->mode2=c;p->mode3=d;
}
/* Originalbereiche :1656-2825, :2939-3370, :3389-3561. */
static void move_state(RECVX_ENEMY_WORK* w);
static void grab_state(RECVX_ENEMY_WORK* w);
static void damage_state(RECVX_ENEMY_WORK* w);
static void player_grab(RECVX_ENEMY_WORK* w,RECVX_ENEMY_PLAYER* p);
static void player_death(RECVX_ENEMY_WORK* w,RECVX_ENEMY_PLAYER* p);

void recvx_en04_init_work(RECVX_ENEMY_WORK* w) { /* en04.c:939 */
    int r;recvx_en04_reset_shared();
    w->ar=3;w->ah=4;w->aw=w->ad=0;w->car=3.5f;w->cah=4;w->stflg=0;
    if(!data.hp_tbl) {gap(w,"en04_hp_tbl");return;}
    if(!random_value(w,&r))return;
    w->hp=data.hp_tbl[context.game_mode==2][r%16];
    memset(w->dam,0,sizeof(w->dam));w->hokan_rate=0;w->hokan_count=0;
    w->mtn_no=0;w->mtn_add=0;w->frm_no=0;w->mtn_tp=recvx_en04_flip;w->mtn_md=0;
    if(!w->exp0) {
        NEED(calloc_work,"bhEne_CallocWork");w->exp0=w->sv->calloc_work(sizeof(RECVX_EN04_EXP0));
        if(!w->exp0) {gap(w,"bhEne_CallocWork exhausted (EN04)");return;}
    }else memset(w->exp0,0,sizeof(RECVX_EN04_EXP0));
    ex(w)->flg|=0x21;w->flg=(w->flg|0x178)&~2u;w->cpcl=recvx_en04_capsules;
    if(!(w->flg&0x800)) {
        NEED(set_shadow,"bhSetShadow");w->sv->set_shadow(recvx_en04_shadow,w,1,3,3,6);w->flg|=0x800;
    }
    w->mdflg&=~0x400u;
    XNEED(set_material_color,"npSetAllMatColor");context.en04->set_material_color(w,0xffb2b2b2);
    const int8_t clip[8]={4,5,6,9,15,15,-1,0};memcpy(w->clp_jno,clip,8);
    mode(w,1,0,0,0);w->lok_jno=4;
}
void recvx_en04_set_collision(RECVX_ENEMY_WORK* w) { /* :600 */
    RECVX_ENEMY_VEC a,b,p,local={0,0,-3};
    if(!joint(w,1,&a)||!joint(w,4,&b))return;
    w->watr.c1=a;w->watr.c2=b;w->watr.r=2;
    transform(position(w),w->ax,w->ay,w->az,local,&p);w->aox=p.x-w->px;w->aoz=p.z-w->pz;
}
void recvx_en04_player_link(RECVX_ENEMY_WORK* w) { /* :825 */
    if(!(ex(w)->flg&0x80000))return;
    RECVX_ENEMY_PLAYER p;RECVX_ENEMY_VEC q;if(!player(w,&p))return;
    transform(position(w),w->ax,w->ay,w->az,ex(w)->link,&q);
    p.pos.x=q.x;p.pos.z=q.z;p.ay=(w->ay+w->waxp)&65535;put_player(w,&p);
}
void recvx_en04_player_control(RECVX_ENEMY_WORK* w) { /* :816 */
    if(!(ex(w)->flg&0x40000))return;
    RECVX_ENEMY_PLAYER p;if(!player(w,&p))return;
    if(p.mode0!=4&&p.mode0!=6)return;
    if(p.mode1==0)player_grab(w,&p);
    else if(p.mode1==1)player_death(w,&p);else gap(w,"bhEne04_PlyDmgMode index");
}
void recvx_en04_coll_check_wall(RECVX_ENEMY_WORK* w) { /* :855 */
    RECVX_EN04_EXP0* e=ex(w);
    if((e->flg&0x10)||!(w->flg&0x10))return;
    RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR rec;RECVX_ENEMY_VEC to,ps,ops,pd;int hit;
    if(!player(w,&p))return;
    w->stflg&=~1u;to=p.pos;
    if(!line_check(w,position(w),&to,&rec,&hit))return;
    e->line_hit=hit;e->line_type_valid=hit!=0;if(hit)e->line_type=rec.type;
    e->wall_hist=(e->wall_hist<<1)&14;
    NEED(wall_type,"bhCheckWallType");ps=position(w);
    e->wall_hit=w->sv->wall_type(&ps,w->flg,w->ar,w->ah);
    if(e->wall_hit)e->wall_hist|=1;
    switch(e->flg&15) {
    case 1:
        NEED(wall_move,"bhCheckWallEx");ops=(RECVX_ENEMY_VEC){w->pxb,w->pyb,w->pzb};
        w->sv->wall_move(w,&ps,&ops,w->ar,w->ah,w->flg);w->px=ps.x;w->py=ps.y;w->pz=ps.z;
        if(!parts(w,&ps)||!joint(w,5,&ops))return;
        ps.y=w->py;ops.y=w->pyb;XNEED(collision_check_wall2,"bhEne_CollisionCheckWall2");
        hit=context.en04->collision_check_wall2(w,&ps,&ops,&pd,2.5f,w->ah);
        if(hit)e->wall_hit=hit;w->px+=pd.x;w->py+=pd.y;w->pz+=pd.z;break;
    case 2:
        XNEED(player_collision_parts,"bhEne_CollisionCheckWall2(plp)");
        if(!context.en04->player_collision_parts(1,0,&pd)) {gap(w,"player CollisionCheckWall2 root");return;}
        w->px+=pd.x;w->pz+=pd.z;
        if(!context.en04->player_collision_parts(6,5,&pd)) {gap(w,"player CollisionCheckWall2 head");return;}
        w->px+=pd.x;w->pz+=pd.z;break;
    }
}
void recvx_en04_collision_check(RECVX_ENEMY_WORK* w) { /* :842 */
    if(!(w->flg&2)) {
        if(ex(w)->flg&0x20) {
            RECVX_ENEMY_PLAYER p;if(!player(w,&p))return;
            if(p.hp>=0) {NEED(check_player,"bhCheckPlayer");w->sv->check_player(w);}
        }
        NEED(check_enemies,"bhCheckEnemies");w->sv->check_enemies(w);
    }
    recvx_en04_coll_check_wall(w);
}
void recvx_en04_damage_add(RECVX_ENEMY_WORK* w) { /* :733 */
    const RECVX_EN04_WPNDAMAGE* t=&recvx_en04_wpn_damage[0];RECVX_EN04_EXP0* e=ex(w);
    if(w->hp>=0) {
        if(getenv("EN04_DMG_DBG"))fprintf(stderr,"EN04DMG id=%u total_dam=%d comb_wep=%d wpnr=%d hp_before=%d\n",w->id,w->total_dam,w->comb_wep,w->wpnr_no,w->hp);
        w->hp-=w->total_dam;
        if(w->comb_wep>=22) {gap(w,"En04_WpnDamageTbl comb_wep");return;}
        t=&recvx_en04_wpn_damage[w->comb_wep];
        recvx_en04_se_play(w,w->hp<0?16786185:16847623);
        NEED(obj_num,"mlwP->obj_num");unsigned n=w->sv->obj_num(w);
        if(n>64) {gap(w,"dam/object count (EN04)");return;}
        for(unsigned i=1;i<n;i++)if(w->dam[i]>0) {
            w->djnt_no=i;if(t->flg&8)continue;
            XNEED(set_blood,"bhEne_SetBlood");
            context.en04->set_blood(w,((w->comb_flg&1)||w->hp<0)?t->cb_blood:t->nm_blood,recvx_en04_blood);
        }
        if((t->flg&1)&&(w->comb_flg&16)&&w->hp<0) {
            RECVX_ENEMY_VEC p;if(!joint(w,w->djnt_no,&p))return;
            XNEED(nikuhen_effect2,"bhEne_SetNikuhenEffect2");context.en04->nikuhen_effect2(w,2,&p,4,4);
        }
    }
    if((t->flg&6)&&e->ct_18<=0) {
        e->ct_18=10;XNEED(set_dfire_effect,"bhEne_SetDFireEffect");
        context.en04->set_dfire_effect(w,w->djnt_no,recvx_en04_blood,(t->flg&4)?2:1);
        if((t->flg&4)&&w->hp<0) {
            w->mdflg|=0x400;XNEED(set_material_color,"npSetAllMatColor");context.en04->set_material_color(w,0xff000000);
        }
    }
    if((t->flg&16)&&e->ct_18<=0) {
        e->ct_18=10;XNEED(set_san_effect,"bhEne_SetSanEffect");context.en04->set_san_effect(w,w->djnt_no,recvx_en04_blood);
    }
}
void recvx_en04_chg_dmg_mode(RECVX_ENEMY_WORK* w) { /* :688 */
    if(w->wpnr_no<0||w->wpnr_no>=22) {gap(w,"En04_WpnDamageTbl wpnr_no");return;}
    RECVX_EN04_EXP0* e=ex(w);const RECVX_EN04_WPNDAMAGE* t=&recvx_en04_wpn_damage[w->wpnr_no];int act=t->nm_act;
    if((e->flg&0x100000)||w->hp<0) {w->comb_flg|=1;w->comb_timeout=0;w->comb_pnt=0;}
    if(w->comb_flg&1) {
        if(e->flg&0x100000) {
            e->dmg_sub=0;mode(w,3,0,0,2);recvx_en04_chg_mtn(w,14,0,10);e->flg&=~0x100000u;return;
        }act=t->cb_act;
    }
    if((unsigned)act<4) mode(w,3,0,(e->flg&0x20000)?1:0,0);
}
void recvx_en04_dmg_chk(RECVX_ENEMY_WORK* w) { /* :626 */
    if(!(w->flg&4)||(w->flg&2))return;
    /* Vor Init gibt es noch keinen exp0: hier ist original flg&4 aus. */
    if(!w->exp0) {gap(w,"EN04 exp0 before damage");return;}
    XNEED(calc_damage,"bhEne_CalcDamage");context.en04->calc_damage(w,recvx_en04_combo_wep,recvx_en04_combo_joint);
    if(!w->total_dam)return;
    RECVX_ENEMY_PLAYER p;RECVX_EN04_EXP0* e=ex(w);uint32_t wf;
    if(!player(w,&p))return;
    if((e->flg&0x2000000)&&w->flr_no<=p.flr_no) {
        XNEED(weapon_flg,"WpnTab[].flg");
        if(!context.en04->weapon_flg(w->wpnr_no,&wf)) {gap(w,"WpnTab[].flg");return;}
        if(!(wf&0x20000020)&&(p.at_flg&2))return;
    }
    recvx_en04_damage_add(w);w->type=0;
    if(w->mode0>=3)return;
    w->flg|=0x40;e->flg=(e->flg&~0x100u)|((w->comb_flg&4)?0x100:0);
    recvx_en04_chg_dmg_mode(w);
    if(!(e->flg&0x4000000))return;
    e->flg&=~0x40c0000u;p.exp1_flags|=4;
    XNEED(pad_on_clear,"sys->pad_on");context.en04->pad_on_clear(15);
    p.flg=(p.flg&~0x10000u)|8;p.stflg&=~0x50000u;p.at_flg=0;p.mtn_enemy=0;p.mtn_enemy_work=NULL;
    if(p.flg2&0x200) {
        RECVX_ENEMY_VEC dv;int32_t j;
        XNEED(player_hit_get,"plp->dvx");if(!context.en04->player_hit_get(&j,&dv,NULL)) {gap(w,"plp->dvx");return;}
        player_mode(&p,2,recvx_t3_dgc_dir_check(&dv,p.ay)==0?0:1,1,0);p.flg|=4;
    }else {player_mode(&p,1,0,0,0);p.flg&=~4u;}
    put_player(w,&p);
}
void recvx_en04_rot_neck(RECVX_ENEMY_WORK* w,int32_t rx,int32_t ry,int32_t rz) { /* :3997 */
    (void)rx;(void)rz;XNEED(node_ang_set,"mlwP->objP[n].ang");
    context.en04->node_ang_set(w,2,1,ry);context.en04->node_ang_set(w,3,1,ry+ry/4);
    context.en04->node_ang_set(w,4,1,ry+ry/2);
}
int recvx_en04_search_neck(RECVX_ENEMY_WORK* w) { /* :4008 */
    RECVX_ENEMY_PLAYER p;if(!player(w,&p))return 0;
    RECVX_EN04_EXP0* e=ex(w);recvx_en04_rot_neck(w,0,e->neck,0);
    int a=recvx_t3_niten_dir(w->px,w->pz,p.pos.x,p.pos.z);
    int delta=(a-(w->ay+e->neck)+512)&65535;
    if(delta>1024)e->neck+=delta<=32768?256:-256;
    int limit=0;if(e->neck< -4096) {e->neck=-4096;limit=1;}if(e->neck>4096){e->neck=4096;limit=1;}
    return limit;
}
void recvx_en04_check_mtn_tbl(RECVX_ENEMY_WORK* w,int frm) { /* :3860 */
    if(w->mtn_room)return;
    for(unsigned t=0;t<30&&recvx_en04_sfx[t].no!=UINT32_MAX;t++)if(recvx_en04_sfx[t].no==w->mtn_no)
        for(unsigned a=0;a<4&&recvx_en04_sfx[t].atb[a].frm!=UINT32_MAX;a++)
            if(recvx_en04_sfx[t].atb[a].frm==(uint32_t)frm)recvx_en04_se_play(w,recvx_en04_sfx[t].atb[a].act);
    if(w->mode0>=5)return;
    for(unsigned t=0;t<6&&recvx_en04_vib[t].no!=UINT32_MAX;t++)if(recvx_en04_vib[t].no==w->mtn_no)
        for(unsigned a=0;a<4&&recvx_en04_vib[t].atb[a].frm!=UINT32_MAX;a++)
            if(recvx_en04_vib[t].atb[a].frm==(uint32_t)frm)vibration(w,recvx_en04_vib[t].atb[a].act);
}
int recvx_en04_set_mtn(RECVX_ENEMY_WORK* w) { /* :3722 */
    NEEDI(set_motion,"bhSetMotion");int frm=w->frm_no/65536;
    int ret=w->sv->set_motion(w,w->mtn_add,w->mtn_md);
    if(ex(w)->flg&0x40000000) {node_pos(w,0,0);node_pos(w,2,0);recvx_t3_get_translate_mtn(w,frm,0);}
    w->flg=(w->flg&~0x2000000u)|(ret?0x2000000:0);
    if(w->mode0<5) {
        if((w->mtn_no==16&&(frm==5||frm==70))||(w->mtn_no==25&&(frm==4||frm==22||frm==33))||
           (w->mtn_no==43&&(frm==4||frm==26||frm==37))) {
            RECVX_ENEMY_VEC p={0,0,-1};w->djnt_no=5;
            if(context.en04&&context.en04->set_blood2) {
                context.en04->set_blood2(w,1,&p,0);context.en04->set_blood2(w,4,&p,w->ay+32768);
            }else gap(w,"bhEne_SetBlood2");
        }
        int y_zero=0,limit=-1;
        switch(w->mtn_no) {
        case 4:if(frm>=11){y_zero=1;w->flg&=~0x100u;}break;
        case 5:y_zero=1;break;
        case 6:if(frm<10){y_zero=1;w->flg&=~0x100u;}else w->flg|=0x100;break;
        case 8:limit=18;break;case 9:limit=25;break;case 20:limit=17;break;
        case 10:case 11:node_pos(w,0,0);node_pos(w,2,0);break;
        case 12:node_pos(w,1,-3.880015f);break;
        case 39:node_pos(w,0,0);node_pos(w,1,0);node_pos(w,2,0);node_pos(w,1,-3.782267f);break;
        }
        if(limit>=0) {
            node_pos(w,0,0);node_pos(w,2,0);
            if(frm<limit){y_zero=1;w->flg&=~0x100u;}else w->flg|=0x100;
        }
        if(y_zero)node_pos(w,1,0);
    }
    recvx_en04_check_mtn_tbl(w,frm);return ret;
}
void recvx_en04_main_loop(RECVX_ENEMY_WORK* w) { /* :558 */
    switch(w->mode0) {
    case 0:recvx_en04_init_work(w);break;case 1:recvx_en04_move(w);break;
    case 2:recvx_en04_nage(w);break;case 3:recvx_en04_damage(w);break;case 4:recvx_en04_die(w);break;
    case 5:NEED(event,"bhEne_Event");w->sv->event(w);break;
    default:gap(w,"bhEne04_Mode0 index");return;
    }
    if(!w->exp0){gap(w,"EN04 exp0");return;}
    recvx_en04_player_control(w);
    if(!(ex(w)->flg&0x40))recvx_en04_set_mtn(w);
    if(w->mode0==5)return;
    RECVX_EN04_EXP0* e=ex(w);
    if(e->flg&0x80) {recvx_en04_search_neck(w);return;}
    if(!e->neck)return;
    if((e->neck&65535)<32768) {e->neck-=256;if(e->neck<0)e->neck=0;}
    else {e->neck+=256;if(e->neck>=0)e->neck=0;}
    recvx_en04_rot_neck(w,0,e->neck,0);
}
void recvx_en04_shake_wire(RECVX_ENEMY_WORK* w,uint32_t object) { /* :3955 */
    XNEED(object_get,"sys->obwp");XNEED(object_set,"sys->obwp write");
    RECVX_EN04_OBJECT o;if(!context.en04->object_get(object,&o)){gap(w,"sys->obwp[index]");return;}
    if(o.mode3==0) {o.ct0=0;o.mode1=1;o.mode3=1;}
    if(o.mode3==1) {
        if(o.ct0<0||o.ct0>=10){gap(w,"bhEne04_ShakeWire ct0");return;}
        RECVX_ENEMY_VEC d=recvx_en04_wire_offset[o.ct0];
        o.py+=d.y;o.px-=d.z*njSin(o.ay+o.ayp);o.pz-=d.z*njCos(o.ay+o.ayp);
        if(++o.ct0>=10){o.mode3++;o.flg2&=~1u;}
    }else if(o.mode3==2)o.mode1=0;
    context.en04->object_set(object,&o);
}
void recvx_en04(RECVX_ENEMY_WORK* w) { /* :516, Reihenfolge verbindlich */
    if(!w)return;
    recvx_en04_dmg_chk(w);recvx_en04_main_loop(w);
    if(w->flg&4){memset(w->dam,0,sizeof(w->dam));w->flg&=~4u;}
    if(!w->exp0)return;
    recvx_en04_collision_check(w);recvx_en04_player_link(w);
    NEED(calc_model,"bhCalcModel");w->sv->calc_model(w);recvx_en04_set_collision(w);
    XNEED(object_count,"rom->obj_n");XNEED(object_get,"sys->obwp");
    unsigned n=context.en04->object_count();
    for(unsigned i=4;i+1<n;i++) {
        RECVX_EN04_OBJECT o;if(!context.en04->object_get(i,&o)){gap(w,"sys->obwp[index]");return;}
        if((o.flg&1)&&o.id==52&&(o.flg2&1))recvx_en04_shake_wire(w,i);
    }
    if(ex(w)->ct_14>0)ex(w)->ct_14--;if(ex(w)->ct_18>0)ex(w)->ct_18--;
}

/* Sichtzyklen sind 4 pruefende + 12 ruhende Ticks, nicht Dauerverfolgung. */
void recvx_en04_ene_search(RECVX_ENEMY_WORK* w) {
    RECVX_EN04_EXP0* e=ex(w);e->search|=128;
    if((e->search&31)<4) {
        RECVX_ENEMY_PLAYER p;if(!player(w,&p))return;
        NJS_MATRIX m;NJS_VECTOR f={0,0,-1},a,b,d={w->px-p.pos.x,w->py-p.pos.y,w->pz-p.pos.z};
        njUnitMatrix(&m);njRotateY(&m,w->ay);njCalcVector(&m,&f,&a);
        njRotateY(&m,18204);njCalcVector(&m,&f,&b);njUnitVector(&d);
        float threshold=-(a.x*b.x+a.y*b.y+a.z*b.z);
        if(a.x*d.x+a.y*d.y+a.z*d.z<threshold &&
           (!e->line_hit||(e->line_type_valid&&(e->line_type==1||e->line_type==3||e->line_type==5)))) e->search|=32;
        if((e->search&31)==3)e->search=(e->search&~64u)|((e->search&32)?64:0),e->search&=95;
    }
    e->search++;if((e->search&31)>15)e->search&=224;
}
int recvx_en04_kaidan_check(RECVX_ENEMY_WORK* w) {
    RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR a;int idx;if(!player(w,&p))return 0;
    if(p.stflg&0x80000000)return 0;
    NEEDI(enemy_count,"sys->ewk_n");NEEDI(enemy_at,"ene[index]");
    unsigned n=w->sv->enemy_count();
    for(unsigned i=0;i<n;i++) {
        RECVX_ENEMY_WORK* other=w->sv->enemy_at(i);
        if(!other){gap(w,"ene[index]");return 0;}
        if(other!=w&&other->id==4&&(other->flg&1)&&!(other->flg&2)&&other->exp0&&(ex(other)->flg&0x20000))return 0;
    }
    XNEEDI(stair_area_atr,"bhKaidanAtrCheck");
    if(!context.en04->stair_area_atr(w,7,&a,&idx)||a.prm2!=4)return 0;
    if((a.prm0==0&&p.flr_no>w->flr_no)||(a.prm0!=0&&p.flr_no<w->flr_no)) {
        NEEDI(stair_use,"bhSetUseKaidanFlag");
        ex(w)->stair_rec=a;ex(w)->stair_idx=idx;ex(w)->stair_atr=1;
        w->sv->stair_use(idx,1);return 1;
    }return 0;
}
static int player_scripted_run(const RECVX_ENEMY_PLAYER* p) {return p->mode1==1&&p->mode2==69;}
static void attack_mode(RECVX_ENEMY_WORK* w,unsigned m) {w->mode1=0;w->mode2=m;w->mode3=0;}
void recvx_en04_brain(RECVX_ENEMY_WORK* w) {
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_VEC q;RECVX_ENEMY_ATR a;int hit,r;
    recvx_en04_ene_search(w);if(!player(w,&p))return;
    if(!(e->search&128)) {
        if((e->search&64)&&e->dist<50&&!e->line_hit&&w->flr_no==p.flr_no)q=p.pos;
        else {
            XNEED(check_route,"bhCheckRoute");RECVX_ENEMY_VEC from=position(w);
            /* rutchk.c:5 writes `pos` only for a valid zone pair; without route
               zones the original continues with the uninitialised stack value
               of the previous call. The last target is that value here; the
               brain pass itself goes on as in the original. */
            if(!context.en04->check_route(&from,&p.pos,&q)){gap(w,"bhCheckRoute: keine Wegezone (Original liest uninitialisiertes pos)");q.x=e->target.x;q.y=p.pos.y;q.z=e->target.z;}
        }e->target.x=q.x;e->target.z=q.z;
    }
    switch(w->mode2) {
    case 0:
        if((!recvx_t3_ikou3(w,&p.pos,12288)&&e->dist<35)||player_scripted_run(&p)||(p.mode2==6&&e->dist<40))e->flg|=0x400000;
        break;
    case 1:
        if((!recvx_t3_ikou3(w,&p.pos,8192)&&e->dist<20)||player_scripted_run(&p)||(p.mode2==6&&e->dist<40)||(e->flg&0x8000))chase(w);
        break;
    case 2:
        if(e->dist<20&&!(e->flg&0x8000)&&!(p.flg&4)&&w->flr_no==p.flr_no) {
            if(e->dist>11) {
                if(!recvx_t3_ikou3(w,&p.pos,1024)) {
                    int back=recvx_t3_cdir_check(w->ay,p.ay);
                    if(!back||(back==1&&p.mode2!=6)) {
                        q=p.pos;if(!line_check(w,position(w),&q,&a,&hit))return;if(!hit)attack_mode(w,7);
                    }
                }
            }else if(!recvx_t3_ikou3(w,&p.pos,2048)) {
                int back=recvx_t3_cdir_check(w->ay,p.ay);
                if(!(e->flg&0x200000)&&!p.debug_attack_disabled&&!(p.flg&6)&&!(p.stflg&0x80000000)&&back==0) {
                    mode(w,2,0,0,0);p.flg|=0x10004;e->flg|=0x4000000;put_player(w,&p);return;
                }attack_mode(w,9);
            }
            if(!recvx_t3_ikou3(w,&p.pos,16384))e->flg|=0x2000;
            else if(e->flg&0x2000) {
                if(!random_value(w,&r))return;if(r%64>56)e->flg=(e->flg&~0x2000u)|0x1800;
            }
        }
        if(e->dist>=8&&e->dist<=15&&(p.flg&4)&&!recvx_t3_ikou3(w,&p.pos,4096))attack_mode(w,4);
        if(recvx_en04_kaidan_check(w)) {e->flg|=0x20000;attack_mode(w,11);e->link=position(w);}break;
    case 4:
        if(p.hp<0) {attack_mode(w,12);e->flg&=~0x2000000u;break;}
        if(e->dist<20&&!(e->flg&0x8000)&&!(p.flg&4)&&w->flr_no==p.flr_no) {
            if(e->dist>11) {
                if(!recvx_t3_ikou3(w,&p.pos,1024)) {
                    q=p.pos;if(!line_check(w,position(w),&q,&a,&hit))return;
                    if(!hit){attack_mode(w,7);e->flg&=~0x2000000u;}
                }
            }else if(!recvx_t3_ikou3(w,&p.pos,2048)){attack_mode(w,9);e->flg&=~0x2000000u;}
        }break;
    case 6: {
        float speed;XNEED(player_speed,"plp->spd");
        if(!context.en04->player_speed(&speed)){gap(w,"plp->spd");return;}
        if(e->dist<11) {
            w->ct1++;if(speed>=.9f)w->ct1+=20;
            if(!recvx_t3_ikou3(w,&p.pos,2048)&&!(p.flg&4)&&w->flr_no==p.flr_no&&e->dist>8) {attack_mode(w,7);contact_flags|=2;}
        }else {
            w->ct1-=w->ct2>0;if(!recvx_t3_ikou3(w,&p.pos,2048)&&speed>=.9f)w->ct1+=6;
        }
        if(w->ct1>=65||player_scripted_run(&p)||(contact_flags&2)||(e->flg&0x8000))chase(w);
        break;
    }
    case 3:case 5:case 7:case 8:case 9:case 10:case 11:break; /* originale Dummy-Faelle */
    default:gap(w,"bhEne04_BrainMode2 index (12 has no entry)");break;
    }
}
/* -1 ist kein Spielwert, sondern ausdruecklich nicht ausfuehrbare Semantik.
   Die Originalroutine gibt 0..2 zurueck; Caller duerfen -1 nicht rendern. */
int recvx_en04_escape(RECVX_ENEMY_WORK* w,int res,int step) {
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;int d,hit;
    if(!player(w,&p))return -1;
    switch(e->u48.esc_mode) {
    case 0:
        w->wax=(int16_t)recvx_t3_ikou3(w,&p.pos,step);if(!w->wax)w->wax=step;
        e->flg&=~0x4000u;e->esc_ct=0;e->u48.esc_mode=1;
        /* fall through */
    case 1:
        if(!e->wall_hit){gap(w,"bhEne04_Escape mode1 undefined return register (SLUS)");return -1;}
        e->u48.esc_mode++;d=w->wax;if(e->esc_ct>8)e->esc_ct=8;break;
    case 2:
        if(!(e->wall_hist&15)) {
            if(!side_wall(w,3,1,&hit))return -1;
            e->u48.esc_mode++;e->esc_turn=0;w->waz=hit?hit:-1;w->waz=-w->waz;
        }
        w->ay+=w->wax;d=w->wax;if(e->esc_ct>8)e->esc_ct=8;break;
    case 3:
        if(!dir_wall(w,w->waz*16384,3,&hit))return -1;
        d=0;
        if((!hit&&!e->esc_turn)||(e->wall_hist&1)) {
            e->u48.esc_mode=4;w->wax=w->waz;e->esc_turn=16384;
        } else {
            e->esc_turn-=e->esc_turn>0;
        }
        break;
    case 4:
        w->ay+=w->wax*step;e->esc_turn-=step;if(e->esc_turn<=0){e->u48.esc_mode=3;e->esc_turn=4;}d=w->wax;break;
    default:gap(w,"bhEne04_Escape state undefined return");return -1;
    }
    int a=recvx_t3_niten_dir(w->px,w->pz,p.pos.x,p.pos.z);
    if(!dir_wall(w,a,4,&hit))return -1;
    if(!hit&&e->esc_ct>12){w->waz=0;e->flg|=0x4000;}
    if(++e->esc_ct>res)e->u48.esc_mode=0;
    return (d>0)-(d<0)+1;
}
/* Maskenbreite 32 am MIPS-sllv ist durch die Instruktion belegt; nur die
   nativen Spielbits abbilden, keine CPU-/Registerwelt aufbauen. */
static uint32_t area_bit(unsigned tag) {return UINT32_C(1)<<(tag&31);}
static int teleport_area(RECVX_ENEMY_WORK* w,const RECVX_ENEMY_ATR* a,int room_bit) {
    if(a->prm2>=4){gap(w,"kaidan_ang[ATR.prm2]");return 0;}
    if(room_bit&&(!w->sv||!w->sv->system_flags_or)){gap(w,"sys->rm_flg");return 0;}
    w->px=a->px+a->w/2;w->pz=a->pz+a->d/2;w->ay=recvx_en04_stair_angle[a->prm2];
    if(room_bit)w->sv->system_flags_or(1,area_bit(a->prm3+16));return 1;
}
static int activation_check(RECVX_ENEMY_WORK* w,int alternate) {
    RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR trigger,dest,chosen;int source,r,hit;unsigned count=0;
    if(!player(w,&p))return 0;
    hit=area(w,p.pos,p.flr_no,6,&trigger,&source);if(hit<=0)return 0;
    if(trigger.prm3>32)return 0;
    if(!alternate) {
        uint32_t bit=area_bit(trigger.prm3);if(area_flags&bit)return 0;area_flags|=bit;
    }
    XNEEDI(flr_count,"rom->flr_n/sys->mflr_n");XNEEDI(flr_at,"flrp");
    unsigned n=context.en04->flr_count();
    for(unsigned i=0;i<n;i++) {
        RECVX_ENEMY_ATR a;if(!context.en04->flr_at(i,&a)){gap(w,"flrp[index]");return 0;}
        if((int)i!=source&&(a.flg&1)&&a.type==2&&a.flr_no==w->flr_no&&a.prm0==w->id&&a.prm1==6&&
           (alternate||!(area_flags&area_bit(a.prm3))))count++;
    }
    if(!alternate) {
        if(count){if(!random_value(w,&r))return 0;if(r%2)return 0;}
        if(area_tag(w,7,trigger.prm3,&dest)!=1)return 0;
        if(!teleport_area(w,&dest,1))return 0;contact_flags|=1;return 1;
    }
    if(!count)return 0;
    if(count>32){gap(w,"bhEne04_AtariCheck2 original work[32] overflow");return 0;}
    unsigned choice=0;if(count>1){if(!random_value(w,&r))return 0;choice=r%count;}
    /* Zweiter linearer Durchlauf statt Original-Zeigerarray; gleiche Auswahl,
       gleiche Reihenfolge, keine neue Besitzertabelle. */
    for(unsigned i=0;i<n;i++) {
        RECVX_ENEMY_ATR a;if(!context.en04->flr_at(i,&a)){gap(w,"flrp[index]");return 0;}
        if((int)i!=source&&(a.flg&1)&&a.type==2&&a.flr_no==w->flr_no&&a.prm0==w->id&&a.prm1==6) {
            if(!choice){chosen=a;break;}choice--;
        }
    }
    if(area_tag(w,7,chosen.prm3,&dest)!=1)return 0;
    /* Original schreibt Lage vor dem dritten Zufallsaufruf. */
    if(!teleport_area(w,&dest,0))return 0;
    if(!random_value(w,&r))return 0;
    if(r%3==0){NEEDI(system_flags_or,"sys->rm_flg");w->sv->system_flags_or(1,area_bit(dest.prm3+16));}
    return 1;
}
int recvx_en04_atari_check(RECVX_ENEMY_WORK* w){return activation_check(w,0);}
int recvx_en04_atari_check2(RECVX_ENEMY_WORK* w){return activation_check(w,1);}

/* ===========================================================================
   en04.c:1020 bhEne04_Move und :1048/:1117/:1152 bhEne04_MVType00..02
   =========================================================================== */

/* Alle drei Typzweige bestimmen zuerst EXP0_F(0x20) als njDistanceP2P
   zwischen (owP[1].mtx[12], epw->py, owP[1].mtx[14]) des SPIELERS und der
   eigenen Lage (en04.c:1061/1126/1160). Weil die y-Komponente dabei epw->py
   ist, bleibt nur der waagerechte Anteil. */
static int mv_type_distance(RECVX_ENEMY_WORK* w) {
    RECVX_ENEMY_VEC q;float dx,dz,m[16];
    /* Wie en01.c:2840: vor dem ersten bhCalcModel(plp) liest das Original hier
       nicht initialisierten owP-Speicher (dread.c:161); gemeldet statt erfunden. */
    if(!w->sv||!w->sv->player_joint_matrix||!w->sv->player_joint_matrix(1,m)) {gap(w,"plp->mlwP->owP[1] (en04.c:1061)");return 0;}
    q.x=m[12];q.y=m[13];q.z=m[14];
    dx=q.x-w->px;dz=q.z-w->pz;
    ex(w)->dist=njSqrt(dx*dx+dz*dz);return 1;
}

static void mv_type00(RECVX_ENEMY_WORK* w) { /* en04.c:1048 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_VEC q;
    RECVX_ENEMY_ATR a;int hit;
    if(!mv_type_distance(w)||!player(w,&p))return;
    hit=area(w,p.pos,p.flr_no,0,NULL,NULL);if(hit<0)return;
    e->flg=(e->flg&~0x200000u)|(hit?0x200000:0);
    if(w->mode1==1)recvx_en04_brain(w);
    if(w->mode0!=1)return;
    q.x=w->px-(6.0f*njSin(w->ay));q.z=w->pz-(6.0f*njCos(w->ay));q.y=0.0f;
    hit=area(w,q,w->flr_no,5,NULL,NULL);if(hit<0)return;
    e->flg=(e->flg&~0x8000u)|(hit?0x8000:0);
    if(w->mode2==2||w->mode2==3) {
        hit=area(w,q,w->flr_no,4,&a,NULL);if(hit<0)return;
        if(hit) {
            RECVX_EN04_OBJECT o;
            XNEED(object_get,"sys->obwp");XNEED(object_set,"sys->obwp write");
            if(!context.en04->object_get(a.prm2,&o)){gap(w,"sys->obwp[ATR.prm2]");return;}
            if(!o.mode1) {
                /* en04.c:1050 ang_tbl ist wertgleich mit kaidan_ang. */
                if(a.prm3>=4){gap(w,"ang_tbl[ATR.prm3]");return;}
                o.ayp=recvx_en04_stair_angle[a.prm3];o.mode3=0;o.flg2|=1;
                context.en04->object_set(a.prm2,&o);
                recvx_en04_se_play(w,70415);
            }
        }
    }
    move_state(w);
}

static void mv_type01(RECVX_ENEMY_WORK* w) { /* en04.c:1117 */
    RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR a,b;int hit;uint8_t no;
    if(!mv_type_distance(w)||!player(w,&p))return;
    hit=area(w,p.pos,p.flr_no,1,&a,NULL);if(hit<0)return;
    if(hit) {
        no=a.prm3;
        hit=area(w,position(w),w->flr_no,2,&b,NULL);if(hit<0)return;
        if(hit&&b.prm3==no){w->mode1=1;w->mode2=2;w->mode3=0;w->type=0;}
    }
    if(w->mode0==1)move_state(w);
}

static void mv_type02(RECVX_ENEMY_WORK* w) { /* en04.c:1152 */
    if(!mv_type_distance(w))return;
    if(contact_flags&1) {
        recvx_en04_atari_check2(w);
        w->mode1=1;w->mode2=2;w->mode3=0;w->type=0;
    }else if(recvx_en04_atari_check(w)!=0) {
        w->mode1=1;w->mode2=2;w->mode3=0;w->type=0;
    }
    if(w->mode0==1)move_state(w);
}

void recvx_en04_move(RECVX_ENEMY_WORK* w) { /* en04.c:1020 */
    if(!w->exp0){gap(w,"EN04 exp0");return;}
    ex(w)->flg&=~0x80u;  /* en04.c:1022 */
    switch(w->type) {
    case 0:mv_type00(w);break;case 1:mv_type01(w);break;case 2:mv_type02(w);break;
    default:gap(w,"bhEne04_MoveType index");break;
    }
}
int recvx_en04_ply_damage_check(RECVX_ENEMY_WORK* w, int type) {
    RECVX_ENEMY_PLAYER pl;
    if (!player(w, &pl)) return 0;
    if ((pl.flg & 4u) || (pl.debug_attack_disabled || (pl.flg & 2u)) || (pl.stflg & 0x80000000u)) return 0;
    RECVX_ENEMY_VEC center;
    if (!parts(w, &center)) return 0;
    RECVX_ENEMY_CAPSULE pl_cap;
    if (context.en04 && context.en04->player_capsule) {
        if (!context.en04->player_capsule(&pl_cap)) return 0;
    } else {
        gap(w, "player_capsule");
        return 0;
    }
    NEEDI(sphere_capsule, "npCollisionCheckSC");
    if (!w->sv->sphere_capsule(&center, 1.2f, &pl_cap.c1, &pl_cap.c2, pl_cap.r)) return 0;
    int hit = (recvx_t3_cdir_check(pl.ay, w->ay) == 0) ? 1 : 0;
    if (type == 1 && pl.hp < 12) return 2;
    RECVX_ENEMY_VEC pos = {0.0f, 0.0f, 0.0f};
    if (context.en04 && context.en04->player_hit_check_parts) {
        context.en04->player_hit_check_parts(&center);
    } else gap(w, "player_hit_check_parts");
    int rot;
    if (hit == 1) {
        rot = pl.ay;
        pos.z = -1.5f;
    } else {
        rot = (pl.ay + 32768) & 65535;
        pos.z = 1.5f;
    }
    if (context.en04 && context.en04->player_set_blood2) {
        context.en04->player_set_blood2(6, &pos, rot);
    } else gap(w, "player_set_blood2");
    vibration(w, 1);
    pl.flg |= 4u;
    pl.mode0 = 2;
    pl.mode2 = 1;
    pl.mode3 = 0;
    pl.mode1 = (hit != 0) ? 0 : 1;
    put_player(w, &pl);
    return 1;
}

static void player_grab(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl) {
    uint32_t ply = (uint32_t)(pl->ply_id < 4 ? pl->ply_id : 0);
    switch (pl->mode3) {
    case 0:
        pl->mtn_enemy = 1;
        pl->mtn_enemy_owner = w->id;
        pl->mtn_enemy_work = w;
        pl->frm_no = 0;
        pl->hokan_count = 5;
        pl->mtn_add = 65536;
        pl->mtn_md = 0;
        pl->flg &= ~0x40000u;
        pl->mtn_no = (uint32_t)recvx_en04_player_mtn[ply];
        ex(w)->link = recvx_en04_player_offset[ply];
        w->waxp = -32768;
        pl->mode3++;
        break;
    case 1:
        if (pl->frm_no / 65536 == 12) {
            ply_voice(w, 1026);
        }
        if (pl->frm_no / 65536 == 0) {
            pl->frm_no = 0;
            pl->hokan_count = 0;
            pl->mtn_no = (uint32_t)recvx_en04_player_mtn[ply] + 1;
            pl->mode3++;
        }
        break;
    case 2:
        break;
    case 3:
        pl->frm_no = 0;
        pl->hokan_count = 0;
        pl->mtn_no = (uint32_t)recvx_en04_player_mtn[ply] + 2;
        pl->mode3++;
        break;
    case 4:
        if (pl->frm_no / 65536 == 0) {
            RECVX_ENEMY_VEC root;
            if (ply_joint(w, 0, &root)) {
                pl->pos.x = root.x;
                pl->pos.z = root.z;
            }
            if (context.en04 && context.en04->player_exp0_float_set) {
                context.en04->player_exp0_float_set(0x48, pl->pos.x);
                context.en04->player_exp0_float_set(0x50, pl->pos.z);
            } else gap(w, "PEXP0_F(0x48/0x50)");
            if (context.en04 && context.en04->player_node_pos_set) {
                context.en04->player_node_pos_set(0, 0, 0.0f);
                context.en04->player_node_pos_set(0, 2, 0.0f);
            } else gap(w, "plp->mlwP->objP[0].pos");
            pl->mtn_enemy = 0;
            pl->mtn_enemy_work = NULL;
            pl->frm_no = 0;
            pl->mtn_no = 42;
            pl->hokan_rate = 58982;
            pl->hokan_count = 1;
            pl->mtn_add = 0;
            pl->mode3++;
        }
        break;
    case 5:
        if (pl->hokan_count == 0) {
            if (context.en04 && context.en04->pad_on_clear) {
                context.en04->pad_on_clear(0xFu);
            } else gap(w, "sys->pad_on");
            pl->flg &= ~0x10000u;
            pl->stflg &= ~0x10000u;
            pl->at_flg = 0;
            player_mode(pl, 1, 0, 0, 0);
            pl->flg &= ~4u;
            ex(w)->flg &= ~0x4040000u;
        }
        break;
    }
    put_player(w, pl);
}

static void player_death(RECVX_ENEMY_WORK* w, RECVX_ENEMY_PLAYER* pl) {
    uint32_t ply = (uint32_t)(pl->ply_id < 4 ? pl->ply_id : 0);
    int frm;
    switch (pl->mode3) {
    case 0:
        if (ex(w)->flg & 0x200u) {
            pl->mtn_no = (uint32_t)recvx_en04_player_mtn[ply] + 3;
            ex(w)->link.x = 0.000104f;
            ex(w)->link.y = 0.0f;
            ex(w)->link.z = -5.032532f;
            w->waxp = -32768;
        } else {
            pl->mtn_no = (uint32_t)recvx_en04_player_mtn[ply] + 4;
            ex(w)->link.x = -0.785082f;
            ex(w)->link.y = 0.0f;
            ex(w)->link.z = -4.901211f;
            w->waxp = 0;
        }
        pl->exp1_flags |= 0x1E0u;
        pl->exp1_flags &= ~4u;
        pl->stflg |= 0x40000u;
        pl->hp = -1;
        pl->flg &= ~0x40000u;
        pl->mtn_enemy = 1;
        pl->mtn_enemy_owner = w->id;
        pl->mtn_enemy_work = w;
        pl->frm_no = 0;
        pl->hokan_count = 5;
        pl->mtn_add = 65536;
        pl->mtn_md = 0;
        pl->mode3++;
        break;
    case 1:
        frm = pl->frm_no / 65536;
        if (frm == 3) {
            ply_voice(w, 1025);
        }
        if (ex(w)->flg & 0x200u) {
            if (frm == 20 || frm == 29) recvx_en04_se_play(w, 70408);
        } else {
            if (frm == 9 || frm == 15 || frm == 22) recvx_en04_se_play(w, 70408);
        }
        if (w->sv && w->sv->motion_frames) {
            uint32_t n = w->sv->motion_frames(w, pl->mtn_no);
            if (n && (frm == (int32_t)n - 1)) {
                pl->mtn_add = 0;
                pl->flg &= ~8u;
                pl->flg |= 2u;
            }
        }
        break;
    case 2:
        break;
    }
    put_player(w, pl);
}

/* ===========================================================================
   Bewegung, Griff, Schaden, Tod — bhEne04_MV00..MV12, NG00/01, DG00/01, DD00
   und die Spielerzweige PlyDG00/PlyDG01/PlyDamageCheck. Jeder Zweig folgt
   dem Originalablauf einschliesslich der fehlenden `break` (Durchfall) und
   der Reihenfolge der Seiteneffekte.
   =========================================================================== */

/* en04.c:3657 bhEne04_RunMotion wird von MV02 und MV08 mit dem Drehschritt
   des Passes gerufen. en04.c:1825 setzt in genau einem Zweig mode3 auf 2,
   ohne `rot` zuvor zu belegen; das Original liest dort ein undefiniertes
   Register. Dieser Pass meldet die Luecke und laesst die Motion stehen,
   statt einen Wert zu erfinden. */

static void mv00(RECVX_ENEMY_WORK* w) { /* en04.c:1656 */
    RECVX_EN04_EXP0* e=ex(w);int r;
    switch(w->mode3) {
    case 0:
        if(!random_value(w,&r))return;
        recvx_en04_chg_mtn(w,w->type==0?47:49,0,7);
        w->ct0=(r%128)+100;w->mode1=1;w->mode3++;
        /* en04.c:1673: kein break */
    case 1:
        e->flg|=0x80;e->neck=4096;w->ct0--;
        if((w->ct0<=0||(e->flg&0x400000))&&w->type==0) {
            recvx_en04_chg_mtn(w,48,0,10);
            if(!(e->flg&0x400000)){e->flg&=~0x80u;e->neck=0;}
            w->mode3++;
        }
        break;
    case 2:
        if(e->flg&0x400000)e->flg|=0x80;
        if(last_frame(w)) {
            w->mode1=1;w->mode2=(e->flg&0x400000)?2:1;w->mode3=0;
        }
        break;
    default:gap(w,"bhEne04_MV00 mode3");break;
    }
}

static void mv01(RECVX_ENEMY_WORK* w) { /* en04.c:1713 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;int r,hit;
    if((e->search&64)&&e->dist<35.0f)e->flg|=0x80;
    switch(w->mode3) {
    case 0:
        if(!player(w,&p)||!random_value(w,&r))return;
        recvx_en04_chg_mtn(w,0,0,7);
        w->way=256;w->ct0=(r%32)+75;w->ct2=0;
        e->target.x=p.pos.x;e->target.z=p.pos.z;
        w->mode3++;
        /* en04.c:1737: kein break */
    case 1:
        w->spd=0.15f;
        if((e->search&64)&&e->dist<35.0f)recvx_t3_ikou(w,&e->target,w->way);
        advance(w,0);
        if(e->wall_hit!=0||w->ct0==0) {
            if(!side_wall(w,10.0f,0,&hit))return;
            if(hit==0) {
                if(!random_value(w,&r))return;
                w->way=(int16_t)((r%2)!=0?256:-256);
            }else w->way=(int16_t)(hit*256);
            w->ct0=16384;w->ct1=256;w->mode3++;
        }else w->ct0--;
        break;
    case 2:
        w->spd=0.08f;advance(w,16);
        w->ay+=w->way;w->ct0-=256;
        if(w->ct0<0) {
            w->mode3=1;
            if(!random_value(w,&r))return;
            w->ct0=(r%32)+75;
        }
        break;
    default:gap(w,"bhEne04_MV01 mode3");break;
    }
    w->ct2++;
    if((w->ct2%90)==0)recvx_en04_se_play(w,16851714);
}

static void mv02(RECVX_ENEMY_WORK* w) { /* en04.c:1792 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;int r,rot=0,ang,have_rot=1;
    switch(w->mode3) {
    case 0:
        if(!player(w,&p))return;
        if(w->mtn_no==6)recvx_en04_chg_mtn(w,1,327680,0);
        else recvx_en04_chg_mtn(w,1,0,7);
        w->way=1152;w->wax=0;w->ct0=0;w->ct1=0;w->ct2=0;w->ct3=0;
        if(e->flg&0x800) {
            if(!random_value(w,&r))return;
            w->ct1=(r%15)+20;
        }
        e->flg&=~0x3000u;e->flg&=~0x4000u;
        w->spd=0.08f;e->target.x=p.pos.x;e->target.z=p.pos.z;
        w->mode3++;
        /* en04.c:1822: kein break */
    case 1:
        if(e->flg&0x1000) {
            /* en04.c:1824-1826 laesst `rot` unzugewiesen in die gemeinsame
               Schlusszeile (en04.c:1962-1966) laufen. Im Original liegt `rot`
               in $s0; bhEne04_MV02 beschreibt $s0 vor dieser Stelle nicht,
               also traegt es den Wert des Aufrufers weiter: bhEne04_MVType00
               setzt $s0 = $sp + 0x58 (SLUS_201.84 0x1A6138) und laesst es bis
               zum jalr 0x1A63A8 nach bhEne04_MoveMode2[] unveraendert.
               bhEne04_RunMotion (en04.c:3657) wertet nur `rot != 0` und
               `rot > 0` aus; ein positiver EE-Stackzeiger ergibt deshalb
               idx 0 = mtn_tbl[0] bzw. mtn_tbl2[0]. rot = 1 bildet genau
               dieses Ergebnis ab. */
            w->mode3=2;rot=1;
        }else {
            speed_up(w,0.1f,1.2f);
            if(w->spd>=1.2f&&w->ct1<10&&w->ct3==0) {
                rot=recvx_t3_niten_dir(w->px,w->pz,e->target.x,e->target.z);
                w->azp=(rot-w->ay)&65535;
                rot=recvx_t3_ikou3(w,&e->target,w->way);
                recvx_t3_ikou(w,&e->target,w->way);
                w->wax=(int16_t)rot;
            }else {
                if(e->wall_hit) {
                    w->wax=(int16_t)((!(e->flg&0x800)?64:0)+recvx_t3_ikou3(w,&e->target,w->way));
                    e->flg&=~0x800u;
                }
                rot=w->wax;w->ay+=rot;
                if(--w->ct1<11){w->ct1=5;e->flg&=~0x800u;}
            }
            if(!(e->search&0x80)) {
                if(!(e->search&0x40)) {
                    if(++w->ct2>=6){w->mode2=6;w->mode3=0;break;}
                }else w->ct2=0;
            }
            ang=recvx_t3_ikou3(w,&e->target,8192);
            if((ang!=0&&!(e->dist<20.0f)&&e->wall_hit!=0)||(e->line_hit!=0&&e->wall_hit!=0)) {
                w->ct0+=2;
                if(w->ct0>20){w->mode1=0;w->mode2=3;w->mode3=0;}
            }else w->ct0-=(w->ct0>0);
            advance(w,0);
        }
        break;
    case 2:
        w->wax=0;w->ct0=0;w->mode3++;
        /* en04.c:1894: kein break */
    case 3:
        rot=recvx_t3_ikou3(w,&e->target,1280);
        advance(w,-rot);
        if(w->ct0==0) {
            speed_down(w,0.07f,0.05f);
            if(w->spd<=0.05f)w->ct0+=1;
        }else speed_up(w,0.07f,1.2f);
        w->ay+=rot;w->wax+=1280;
        if((uint16_t)w->wax>=32768||rot==0) {
            e->flg&=~0x1000u;w->ct0=0;w->wax=0;w->mode3=1;
        }
        break;
    case 4:
        speed_up(w,0.1f,1.2f);advance(w,0);
        rot=recvx_en04_escape(w,90,1152);
        if(rot<0){have_rot=0;break;} /* Escape meldete seine Luecke selbst */
        if(e->flg&0x4000) {
            if(!random_value(w,&r))break;
            w->ct3=((r%3)*2)+8;w->mode3=0;
        }
        break;
    case 5:
        rot=1280;advance(w,-1280);
        if(w->ct0==0) {
            speed_down(w,0.07f,0.05f);
            if(w->spd<=0.05f)w->ct0++;
        }else speed_up(w,0.07f,1.2f);
        w->ay+=1280;w->wax+=1280;
        if((uint16_t)w->wax>=32768) {
            e->search=132;
            if(!random_value(w,&r))break;
            w->ct1=(r%15)+20;w->ct0=0;w->wax=0;w->mode3=0;
        }
        break;
    default:gap(w,"bhEne04_MV02 mode3");return;
    }
    if(!have_rot) {
        gap(w,"bhEne04_MV02 rot undefiniert (en04.c:1825)");return;
    }
    /* en04.c:1952: liegt das Ziel im vorderen 100-Grad-Sektor oder ist es
       naeher als 15 Einheiten, laeuft der Hund gerade. */
    if((((recvx_t3_niten_dir(w->px,w->pz,e->target.x,e->target.z)-w->ay)+9102)&65535)<18204||
       e->dist<15.0f)rot=0;
    run_motion(w,rot);
}

static void mv03(RECVX_ENEMY_WORK* w) { /* en04.c:1970 */
    RECVX_EN04_EXP0* e=ex(w);int r,hit,rot;
    switch(w->mode3) {
    case 0:
        if(!side_wall(w,10.0f,0,&hit))return;
        if(hit==0) {
            rot=recvx_t3_ikou3(w,&e->target,1536);
            if(rot==0) {
                if(!random_value(w,&r))return;
                rot=(r%2)?1536:-1536;
            }
        }else rot=hit*1536;
        w->way=(int16_t)rot;w->ct0=8;w->ct1=0;w->mode3++;
        /* en04.c:1993: kein break */
    case 1:
        w->ay+=w->way;advance(w,0);
        if(w->ct0>0)w->ct0--;
        else if(e->wall_hit==0) {
            w->mode1=1;w->mode2=2;w->mode3=0;w->way=1152;
        }
        break;
    case 2:
        w->ay+=w->way;advance(w,0);
        if(e->wall_hit==0) {
            w->mode1=1;w->mode2=2;w->mode3=0;w->way=1152;break;
        }
        if(w->ct0++>=8){w->ct0=19;w->spd=0.3f;w->mode3++;}
        break;
    case 3:
        speed_down(w,0.1f,0.08f);
        recvx_t3_ikou(w,&e->target,256);
        if(w->ct0--<=0){w->mode1=1;w->mode2=2;w->mode3=0;w->way=1152;}
        break;
    default:gap(w,"bhEne04_MV03 mode3");return;
    }
    run_motion(w,w->way);
}

static void mv04(RECVX_ENEMY_WORK* w) { /* en04.c:2056 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_VEC root;
    switch(w->mode3) {
    case 0:
        recvx_en04_chg_mtn(w,7,0,7);e->flg|=0x40000000;w->mode1=0;w->mode3++;
        /* en04.c:2064: kein break */
    case 1:
        if(!ply_joint(w,0,&root))return;
        w->ay+=recvx_t3_ikou3(w,&root,512);
        if(w->flg&0x2000000) {
            recvx_en04_chg_mtn(w,49,0,0);e->flg&=~0x40000000u;w->mode1=1;w->mode3++;
        }
        break;
    case 2:
        if(!ply_joint(w,0,&root)||!player(w,&p))return;
        w->ay+=recvx_t3_ikou3(w,&root,512);
        if(!(p.flg&4)) {
            w->mode1=1;w->mode2=2;w->mode3=0;e->flg&=~0x2000000u;
        }
        break;
    default:gap(w,"bhEne04_MV04 mode3");break;
    }
}

static void mv05(RECVX_ENEMY_WORK* w) { /* en04.c:2089 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;
    switch(w->mode3) {
    case 0:
        if(w->mtn_no==45)w->ay+=32768;
        if(e->flg&0x8000) {
            recvx_en04_chg_mtn(w,46,0,0);e->flg&=~0x40000000u;
        }else recvx_en04_chg_mtn(w,15,0,0);
        e->flg|=0x100000;w->flg2|=1;w->mode3++;
        /* en04.c:2109: kein break */
    case 1:
        if(!player(w,&p))return;
        w->ay+=recvx_t3_ikou3(w,&p.pos,512);
        if(w->frm_no/65536>=30)e->flg&=~0x100000u;
        if(last_frame(w)) {
            if(e->flg&0x8000) {
                w->mode1=1;w->mode2=2;w->mode3=0;e->flg&=~0x2000000u;
            }else {w->mode1=0;w->mode2=4;w->mode3=0;}
            w->flg2&=~1u;
        }
        break;
    default:gap(w,"bhEne04_MV05 mode3");break;
    }
}

static void mv06(RECVX_ENEMY_WORK* w) { /* en04.c:2137 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;int r,hit,rot;
    e->flg|=0x80;
    switch(w->mode3) {
    case 0:
        if(!player(w,&p))return;
        {int r2;if(!random_value(w,&r)||!random_value(w,&r2))return;
         recvx_en04_chg_mtn(w,0,0,7);
         w->way=(int16_t)(256-((r%7)*16));w->ct0=(r2%32)+45;}
        w->ct1=0;e->target.x=p.pos.x;e->target.z=p.pos.z;e->flg&=~0x4000u;
        w->mode3++;
        /* en04.c:2156: kein break */
    case 1:
        w->spd=0.15f;advance(w,0);w->ay+=w->way;
        if(--w->ct0==0) {
            if(!random_value(w,&r))break;
            w->ct0=(r%32)+15;w->mode3++;
        }
        break;
    case 2:
        advance(w,0);
        if(--w->ct0==0||e->wall_hit!=0) {
            int r2;if(!player(w,&p)||!random_value(w,&r)||!random_value(w,&r2))break;
            w->way=(int16_t)recvx_t3_ikou3(w,&p.pos,256-((r%7)*16));
            /* en04.c:2177/2179: ct0 wird zweimal gesetzt, der zweite gilt. */
            w->ct0=2;w->mode3=1;w->ct0=(r2%32)+45;
        }
        break;
    case 3:
        advance(w,0);recvx_en04_escape(w,600,256);
        if(e->flg&0x4000)w->mode3=0;
        break;
    default:gap(w,"bhEne04_MV06 mode3");return;
    }
    if(w->mode3!=3) {
        if(!player(w,&p))return;
        rot=recvx_t3_niten_dir(w->px,w->pz,p.pos.x,p.pos.z);
        if(!dir_wall(w,rot,8.0f,&hit))return;
        if(hit){e->esc_turn=0;e->esc_ct=0;e->u48.esc_mode=0;w->mode3=3;}
    }
}

static void mv07(RECVX_ENEMY_WORK* w) { /* en04.c:2205 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_VEC pos,pos2,normal;int hit,ret;float g;
    switch(w->mode3) {
    case 0:
        recvx_en04_chg_mtn(w,4,0,7);w->spd=0.4f;w->ct0=11;w->mode3++;
        /* en04.c:2222: kein break */
    case 1:
        advance(w,0);w->ct0--;
        if(w->ct0<=0) {
            w->ct0=0;e->u48.jump=2.2f;e->flg|=0x400;w->spd=1.9f;w->mode3++;
        }
        break;
    case 2:
        advance(w,0);
        w->py+=e->u48.jump-(0.28f*(float)w->ct0);w->ct0++;
        if(!player(w,&p))return;
        if(w->frm_no/65536>=13||(w->flg&0x2000000)) {
            ret=recvx_en04_ply_damage_check(w,1);
            if(ret==2) {
                mode(w,2,0,1,0);e->flg&=~0x400u;e->flg|=0x4000000;
                if(!player(w,&p))return;
                p.flg|=4;p.flg|=0x10000;put_player(w,&p);break;
            }else if(ret==1) {
                recvx_en04_chg_mtn(w,9,0,0);w->mode1=0;w->mode2=8;w->mode3=0;
                if(!player(w,&p))return;
                p.hp-=12;put_player(w,&p);break;
            }
        }else {
            if(recvx_en04_ply_damage_check(w,0)!=0) {
                recvx_en04_chg_mtn(w,8,0,5);w->mode1=0;w->mode2=8;w->mode3=2;
                if(!player(w,&p))return;
                p.hp-=6;put_player(w,&p);break;
            }
        }
        if(w->frm_no/65536==17){recvx_en04_chg_mtn(w,5,0,0);w->mode3++;}
        break;
    case 3:
        advance(w,0);
        w->py+=e->u48.jump-(0.28f*(float)w->ct0);w->ct0++;
        if(!joint(w,5,&pos))return;
        pos2.x=pos.x-(6.0f*njSin(w->ay));
        pos2.z=pos.z-(6.0f*njCos(w->ay));
        pos2.y=pos.y;
        ret=recvx_en04_ply_damage_check(w,1);
        if(ret!=0) {
            if(ret==2) {
                mode(w,2,0,1,0);e->flg&=~0x400u;e->flg|=0x4000000;
                if(!player(w,&p))return;
                p.flg|=4;p.flg|=0x10000;put_player(w,&p);break;
            }else if(ret==1) {
                recvx_en04_chg_mtn(w,9,0,0);w->mode1=0;w->mode2=8;w->mode3=0;
                if(!player(w,&p))return;
                p.hp-=12;put_player(w,&p);break;
            }
        }
        if(!line_check_normal(w,pos,&pos2,&rec,&normal,&hit))return;
        if(hit&&!(rec.attr&8)) {
            if(recvx_t3_angle_check(&normal,w->ay,8192)!=0) {
                w->ayp=(int32_t)(10430.381f*atan2f(normal.x,normal.z));
                w->ayp=(w->ayp-w->ay)&65535;
                if(w->ayp>32768)w->ayp=(w->ayp-32768)-32768;
                recvx_en04_chg_mtn(w,20,0,0);w->mode3=6;w->ct0=0;break;
            }
        }
        if(w->ct0>7){recvx_en04_chg_mtn(w,6,0,0);w->mode3++;}
        break;
    case 4:
        advance(w,0);
        w->py+=e->u48.jump-(0.28f*(float)w->ct0);
        if(!grand(w,&g))return;
        if(w->py<=g){w->flg|=0x100;w->py=g;}
        w->ct0++;
        if(w->frm_no/65536==9){w->mode3++;e->flg&=~0x400u;}
        break;
    case 5:
        advance(w,0);speed_down(w,0.2f,1.2f);
        if(last_frame(w)){w->mode1=1;w->mode2=2;w->mode3=0;}
        break;
    case 6:
        if(w->frm_no/65536<8)w->ay+=w->ayp/8;
        w->spd=(float)(2.0-(double)w->ct0*0.1f);
        advance(w,0);w->ct0++;
        if(w->frm_no/65536==11){w->ct0=10;w->spd=1.9f;w->mode3++;}
        break;
    case 7:
        advance(w,32768);speed_down(w,0.2f,1.2f);
        w->py+=e->u48.jump-(0.28f*(float)w->ct0);
        if(!grand(w,&g))return;
        if(w->py<=g){e->flg&=~0x400u;w->flg|=0x100;w->py=g;}
        w->ct0++;
        if(w->frm_no/65536==20) {
            root_yaw_zero(w);recvx_en04_chg_mtn(w,1,0,5);
            w->ay=w->ay+32768;w->mode1=1;w->mode2=2;w->mode3=0;
        }
        break;
    default:gap(w,"bhEne04_MV07 mode3");break;
    }
}

static void mv08(RECVX_ENEMY_WORK* w) { /* en04.c:2415 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_ATR rec;
    RECVX_ENEMY_VEC pos,to;int frm,hit;float jump,g;
    switch(w->mode3) {
    case 0:
        e->flg|=0x800;w->ct0=8;w->ct1=5;e->u48.jump=2.2f;
        w->way=0;w->spd=0.8f;w->car=1.5f;w->mode3++;
        /* en04.c:2433: kein break */
    case 1:
        frm=w->frm_no/65536;
        if(frm<10) {
            w->spd=(float)(1.2000000476837158-(double)w->ct0*0.05);
            advance(w,0);w->ct0++;
        }
        if(frm>=14&&frm<26) {
            if(!grand(w,&g))return;
            w->car=3.5f;
            w->py+=e->u48.jump-(0.28f*(float)w->ct1);
            if(w->py<=g){e->flg&=~0x400u;w->flg|=0x100;w->py=g;}
            else {w->spd=0.8f;advance(w,32768);}
            w->ct1++;
        }
        if(w->frm_no/65536==26) {
            root_yaw_zero(w);recvx_en04_chg_mtn(w,1,0,5);
            w->ay+=32768;w->mode1=1;w->mode2=2;w->mode3=0;
        }
        break;
    case 2:
        if(!player(w,&p)||!grand(w,&g))return;
        e->flg|=0x800;w->ct0=0;w->ct1=10;e->u48.jump=2.5f;
        w->way=0;w->spd=0.8f;
        /* en04.c:2477: der Seitenwandtest laeuft von der SPIELERlage aus;
           das Original verschiebt dafuer die eigene Lage und stellt sie
           danach wieder her. spd bleibt auf 1.0f stehen. */
        pos.x=w->px;pos.y=w->py;pos.z=w->pz;
        w->px=p.pos.x;w->py=p.pos.y;w->pz=p.pos.z;
        w->spd=1.0f;
        if(!side_wall(w,5.0f,3,&hit)) {
            w->px=pos.x;w->py=pos.y;w->pz=pos.z;return;
        }
        w->way=(int16_t)hit;
        w->px=pos.x;w->py=pos.y;w->pz=pos.z;
        e->target.x=w->px-(40.0f*njSin(w->ay));
        e->target.z=w->pz-(40.0f*njCos(w->ay));
        e->target.y=g;
        if(w->way==3)w->way=0;
        else {
            if(w->way==0)w->way=1;
            to=e->target;
            if(!line_check(w,position(w),&to,&rec,&hit))return;
            if(hit)w->way=0;
            if(w->way>0)w->mtn_md|=2;
            w->way=(int16_t)(w->way*546);
        }
        w->mode3++;
        /* en04.c:2518: kein break */
    case 3:
        if(!grand(w,&g))return;
        frm=w->frm_no/65536;
        if(frm>=4&&frm<14)w->ay+=w->way;
        if(frm<13){jump=(float)(0.3-(double)w->ct0*0.02f);w->ct0++;}
        else {jump=e->u48.jump-0.28f*(float)w->ct1;w->ct1++;}
        w->py+=jump;
        if(w->py<=g){e->flg&=~0x400u;w->flg|=0x100;w->py=g;}
        if(frm==26) {
            w->mode3=(w->way==0)?5:4;
            recvx_en04_chg_mtn(w,1,0,5);w->way=1152;w->spd=0.08f;w->ct0=12;
        }
        break;
    case 4:
        if(--w->ct0<0){e->search=132;w->mode1=1;w->mode2=2;w->mode3=1;}
        speed_up(w,0.1f,1.2f);advance(w,0);run_motion(w,0);
        break;
    case 5:
        advance(w,-1280);
        if(w->ct0==0) {
            speed_down(w,0.07f,0.05f);
            if(w->spd<=0.05f)w->ct0++;
        }else speed_up(w,0.07f,1.2f);
        w->ay+=1280;w->wax+=1280;
        if((uint16_t)w->wax>=32768){w->mode1=1;w->mode2=2;w->mode3=0;}
        run_motion(w,1280);
        break;
    default:gap(w,"bhEne04_MV08 mode3");break;
    }
}

/* en04.c:2603/2662: MV09 und MV10 kehren mit demselben Satz in den
   Verfolgungslauf MV02 zurueck. */
static void mv_resume_chase(RECVX_ENEMY_WORK* w,const RECVX_ENEMY_PLAYER* p) {
    RECVX_EN04_EXP0* e=ex(w);int r;
    recvx_en04_chg_mtn(w,1,0,0);
    w->wax=0;w->ct0=0;w->ct1=0;w->ct2=0;
    if(e->flg&0x800) {
        if(!random_value(w,&r))return;
        w->ct1=(r%15)+20;
    }
    e->flg&=~0x3000u;e->flg&=~0x4000u;
    w->spd=0.6f;e->target.x=p->pos.x;e->target.z=p->pos.z;
    w->mode1=1;w->mode2=2;w->mode3=1;
}

static void mv09(RECVX_ENEMY_WORK* w) { /* en04.c:2603 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;int frm;
    switch(w->mode3) {
    case 0:
        recvx_en04_chg_mtn(w,36,0,5);w->spd=0.0f;w->way=-546;
        e->flg|=0x40000000;w->mode3++;
        /* en04.c:2615: kein break */
    case 1:
        frm=w->frm_no/65536;
        if(frm>=4&&frm<14) {
            w->ay+=w->way;
            if(recvx_en04_ply_damage_check(w,0)!=0) {
                if(!player(w,&p))return;
                p.hp-=6;put_player(w,&p);
            }
        }
        if(w->flg&0x2000000) {
            if(!player(w,&p))return;
            e->flg&=~0x40000000u;mv_resume_chase(w,&p);
        }
        break;
    default:gap(w,"bhEne04_MV09 mode3");break;
    }
}

static void mv10(RECVX_ENEMY_WORK* w) { /* en04.c:2662 */
    RECVX_ENEMY_PLAYER p;
    switch(w->mode3) {
    case 0:
        recvx_en04_chg_mtn(w,33,0,5);
        /* en04.c:2668: kein break, mode3 bleibt 0 */
    case 1:
        if(w->frm_no/65536<8)w->ay+=w->ayp/8;
        speed_up(w,0.1f,1.2f);advance(w,0);
        if(w->flg&0x2000000) {
            if(!player(w,&p))return;
            mv_resume_chase(w,&p);
        }
        break;
    default:gap(w,"bhEne04_MV10 mode3");break;
    }
}

static void mv11(RECVX_ENEMY_WORK* w) { /* en04.c:2709 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_VEC head;float g;
    switch(w->mode3) {
    case 0:
        if(e->stair_rec.prm0==0) {
            e->stair_h=0;
            w->yn=w->py+(float)(e->stair_rec.prm2*2);
            e->flg|=0x10000;
        }else {
            e->stair_h=(int8_t)e->stair_rec.prm2;
            w->yn=w->py-(float)(e->stair_rec.prm2*2);
            e->flg&=~0x10000u;
        }
        e->stair_flr=w->flr_no;e->flg|=0x10;
        if(e->stair_rec.prm1>=4){gap(w,"kaidan_ang[ATR.prm1]");return;}
        w->axp=recvx_en04_stair_angle[e->stair_rec.prm1];
        w->ayp=(w->axp-w->ay)&65535;
        if(w->ayp>32768)w->ayp-=65536;
        w->ayp/=10;w->ct0=10;w->mode3++;
        /* en04.c:2740: kein break */
    case 1:
        w->ay+=w->ayp;
        if(--w->ct0<0) {
            recvx_en04_chg_mtn(w,(e->flg&0x10000)?37:38,0,5);
            e->flg|=0x40000000;e->flg|=0x20000;e->flg&=~0x20u;
            w->flg2|=1;e->link=position(w);w->ay=w->axp;w->mode3++;
        }
        break;
    case 2:
        if(e->flg&0x10000) {
            if(!joint(w,5,&head))return;
            w->cah=head.y-w->py;
        }
        if(last_frame(w)) {
            w->flr_no=(int8_t)check_floor_num(w,w->yn);
            if(!grand(w,&g))return;
            w->py=g;w->flg2&=~1u;
            recvx_en04_chg_mtn(w,1,0,10);w->mtn_md|=0x100;
            clr_use_kaidan_flag(w);w->cah=4.0f;
            e->flg&=~0x10u;e->flg&=~0x40000000u;e->flg&=~0x20000u;e->flg|=0x20;
            w->mode1=1;w->mode2=2;w->mode3=0;
        }
        break;
    default:gap(w,"bhEne04_MV11 mode3");break;
    }
}

static void mv12(RECVX_ENEMY_WORK* w) { /* en04.c:2788 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_VEC j1;
    switch(w->mode3) {
    case 0:
        recvx_en04_chg_mtn(w,0,0,7);w->mode3++;
        /* en04.c:2797: kein break */
    case 1:
        if(!ply_joint(w,1,&j1))return;
        w->spd=0.15f;advance(w,0);
        w->ay+=recvx_t3_ikou3(w,&j1,256);
        if(e->dist<=9.0f) {
            recvx_en04_chg_mtn(w,42,0,7);e->flg|=0x40000000;w->mode3++;
        }
        break;
    case 2:
        if(!ply_joint(w,1,&j1))return;
        w->ay+=recvx_t3_ikou3(w,&j1,256);
        if(w->flg&0x2000000) {
            e->flg&=~0x40000000u;recvx_en04_chg_mtn(w,43,0,7);w->mode3++;
        }
        /* en04.c:2821: kein break */
    case 3:break;
    default:gap(w,"bhEne04_MV12 mode3");break;
    }
}

/* en04.c:485 bhEne04_MoveMode2[13]. */
static void move_state(RECVX_ENEMY_WORK* w) {
    switch(w->mode2) {
    case 0:mv00(w);break;case 1:mv01(w);break;case 2:mv02(w);break;
    case 3:mv03(w);break;case 4:mv04(w);break;case 5:mv05(w);break;
    case 6:mv06(w);break;case 7:mv07(w);break;case 8:mv08(w);break;
    case 9:mv09(w);break;case 10:mv10(w);break;case 11:mv11(w);break;
    case 12:mv12(w);break;
    default:gap(w,"bhEne04_MoveMode2 index");break;
    }
}

static void ng00(RECVX_ENEMY_WORK* w) { /* en04.c:2939 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;
    RECVX_ENEMY_VEC local={0,0,-1},dp,hit_pos;
    switch(w->mode3) {
    case 0:
        if(!player(w,&p))return;
        recvx_en04_chg_mtn(w,17,0,5);
        w->ay=recvx_t3_niten_dir(w->px,w->pz,p.pos.x,p.pos.z);
        w->flg&=~0x40u;e->flg|=0x40000;e->flg|=0x80000;
        player_mode(&p,4,0,0,0);put_player(w,&p);
        w->ct0=60;w->mode3++;
        /* en04.c:2963: kein break */
    case 1:
        if(w->frm_no/65536==12) {
            if(!w->sv||!w->sv->joint_point){gap(w,"njCalcPoint(owP[5].mtx)");return;}
            if(!w->sv->joint_point(w,5,&local,&dp)){gap(w,"mlwP->owP[5].mtx (NG00)");return;}
            if(!context.en04||!context.en04->player_hit_set){gap(w,"plp->djnt_no/dpx");return;}
            context.en04->player_hit_set(12,NULL,&dp);
            if(context.en04->player_set_blood)context.en04->player_set_blood(1,NULL);
            else gap(w,"bhEne_SetBlood(plp)");
            recvx_en04_se_play(w,16847622);
        }
        if(w->flg&0x2000000){recvx_en04_chg_mtn(w,18,0,0);w->mode3++;}
        break;
    case 2:
        if(w->frm_no/65536==1) {
            if(!player(w,&p))return;
            p.hp-=8;if(p.hp<0)p.hp=0;put_player(w,&p);
        }
        w->ct0-=lever_check(w)+1;
        if(w->ct0<0&&(w->flg&0x2000000)) {
            recvx_en04_chg_mtn(w,19,0,0);w->mode3++;
            if(!player(w,&p))return;
            p.mode3++;put_player(w,&p);
            w->flg|=0x20;
        }
        break;
    case 3:
        if(w->flg&0x2000000) {
            /* en04.c:3022: die berechnete Lage landet in px/py/pz, danach
               ueberschreiben owP[0] nur x und z — y bleibt der Rechenwert. */
            RECVX_ENEMY_VEC local3={1.633476f,0.0f,-1.806519f},out;
            transform(position(w),w->ax,w->ay,w->az,local3,&out);
            w->px=out.x;w->py=out.y;w->pz=out.z;
            if(!joint(w,0,&hit_pos))return;
            w->px=hit_pos.x;w->pz=hit_pos.z;
            e->flg&=~0x80000u;recvx_en04_chg_mtn(w,14,0,0);w->mode3++;
        }
        break;
    case 4:
        if(last_frame(w)) {
            w->flg|=0x40;w->mtn_add=0;
            mode(w,1,0,5,0);e->flg|=0x2100000;
        }
        break;
    default:gap(w,"bhEne04_NG00 mode3");break;
    }
}

static void ng01(RECVX_ENEMY_WORK* w) { /* en04.c:3052 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;RECVX_ENEMY_VEC root;float g;
    switch(w->mode3) {
    case 0:
        if(!player(w,&p)||!grand(w,&g))return;
        w->py=g;recvx_en04_chg_mtn(w,16,0,5);
        w->ay=recvx_t3_niten_dir(w->px,w->pz,p.pos.x,p.pos.z);
        w->flg&=~0x40u;
        e->flg|=0x40000;e->flg|=0x80000;e->flg&=~0xFu;e->flg|=2;
        player_mode(&p,6,1,0,0);put_player(w,&p);
        w->mode3++;
        /* en04.c:3077: kein break */
    case 1:
        if(w->flg&0x2000000) {
            if(!joint(w,0,&root))return;
            w->px=root.x;w->pz=root.z;
            recvx_en04_chg_mtn(w,25,0,0);
            e->flg&=~0x40C0000u;w->flg|=0x20;w->mode3++;
        }
        /* en04.c:3093: kein break */
    case 2:break;
    default:gap(w,"bhEne04_NG01 mode3");break;
    }
}

/* en04.c:2922 bhEne04_NGType00 — alle drei NageType-Eintraege zeigen hierhin. */
static void grab_state(RECVX_ENEMY_WORK* w) {
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_PLAYER p;
    if(w->mode3==0) {
        if(!player(w,&p))return;
        if(recvx_t3_cdir_check(p.ay,w->ay)==0)e->flg|=0x200;
        else e->flg&=~0x200u;
    }
    switch(w->mode2) {
    case 0:ng00(w);break;case 1:ng01(w);break;
    default:gap(w,"bhEne04_NageMode2 index");break;
    }
}

static void dg00(RECVX_ENEMY_WORK* w) { /* en04.c:3106 */
    RECVX_EN04_EXP0* e=ex(w);uint32_t mtn;int h_cnt,frm;float g;
    switch(w->mode3) {
    case 0:
        w->ct0=0;clr_use_kaidan_flag(w);
        if(e->flg&0x400) {
            w->flg&=~0x100u;e->flg&=~0x8400u;w->py+=3.8f;
            e->u48.jump=1.0f;recvx_en04_chg_mtn(w,12,0,5);
        }else if(e->flg&0x8000) {
            if(e->flg&0x100){w->spd=0.0f;w->way=-28672;mtn=45;}
            else {w->spd=0.0f;w->way=4096;mtn=44;}
            w->mode3=2;
            recvx_en04_chg_mtn(w,mtn,0,5);e->flg|=0x40000000;return;
        }else {
            if(e->flg&0x100){w->spd=0.0f;w->way=-28672;mtn=10;}
            else {w->spd=0.0f;w->way=4096;mtn=11;}
            recvx_en04_chg_mtn(w,mtn,0,5);e->flg&=~0x40000000u;
        }
        w->mode3++;
        /* en04.c:3165: kein break */
    case 1:
        if(!grand(w,&g))return;
        if(w->mtn_no==12) {
            w->py+=(fabsf(e->u48.jump)*njSin(8192))-0.2f*(float)w->ct0;
            if(g>w->py)w->py=g;
            else {w->spd=e->u48.jump*njCos(8192);advance(w,32768);}
            w->ct0++;
        }else {
            frm=w->frm_no/65536;
            if(frm>3) {
                w->spd+=(0.25f-(0.05f*(float)w->ct0));
                if(w->spd<0.0f)w->spd=0.0f;
                advance(w,w->way);w->ct0++;
            }
        }
        if(w->flg&0x2000000) {
            w->flg|=0x100;h_cnt=0;
            if(w->mtn_no==11)w->ay+=32768;
            else if(w->mtn_no==12)h_cnt=5;
            recvx_en04_chg_mtn(w,14,0,h_cnt);w->py=g;w->mode3++;
        }
        break;
    case 2:
        if(last_frame(w)) {
            w->mtn_add=0;
            if(w->hp<0){mode(w,4,0,0,0);break;}
            mode(w,1,0,5,0);e->flg|=0x2100000;
        }
        break;
    default:gap(w,"bhEne04_DG00 mode3");break;
    }
}

static void dg01(RECVX_ENEMY_WORK* w) { /* en04.c:3246 */
    RECVX_EN04_EXP0* e=ex(w);RECVX_ENEMY_VEC root,p1={0.0f,-8.027999f,-22.000042f},p2;
    float dist,px,pz,g;
    switch(w->mode3) {
    case 0:
        if(!joint(w,0,&root))return;
        clr_use_kaidan_flag(w);
        e->flg&=~0x40000000u;e->flg&=~0x20000u;e->flg&=~0x20u;
        w->flg2|=1;
        if(e->stair_rec.prm1>=4){gap(w,"kaidan_ang[ATR.prm1]");return;}
        w->ay=recvx_en04_stair_angle[e->stair_rec.prm1];
        w->py=3.8f+root.y;
        if(e->flg&0x10000) {
            recvx_en04_chg_mtn(w,12,0,5);
            px=w->px-e->link.x;pz=w->pz-e->link.z;
        }else {
            w->flr_no=(int8_t)check_floor_num(w,w->yn);
            recvx_en04_chg_mtn(w,39,0,5);
            transform(e->link,w->ax,w->ay,w->az,p1,&p2);
            px=p2.x-w->px;pz=p2.z-w->pz;
        }
        dist=njSqrt(px*px+pz*pz);
        e->u48.jump=dist*0.033333335f+1.0f;
        w->flg&=~0x100u;w->ct0=0;w->mode3++;
        /* en04.c:3297: kein break */
    case 1:
        if(!grand(w,&g))return;
        w->py+=(fabsf(e->u48.jump)*njSin(8192))-0.2f*(float)w->ct0;
        if(g>w->py)w->py=g;
        else {
            w->spd=e->u48.jump*njCos(8192);
            advance(w,(e->flg&0x10000)?32768:0);
        }
        w->ct0++;
        if(last_frame(w))w->mtn_add=0;
        /* en04.c:3320: das Original vergleicht py mit 0.0f, nicht mit
           rom->grand. Uebernommen wie belegt. */
        if(w->mtn_add==0&&w->py==0.0f) {
            w->flg|=0x100;
            if(!(e->flg&0x10000))w->ay+=32768;
            recvx_en04_chg_mtn(w,14,0,0);w->mode3++;
        }
        break;
    case 2:
        if(last_frame(w)) {
            w->mtn_add=0;w->flg2&=~1u;e->flg&=~0x30u;
            if(w->hp<0){mode(w,4,0,0,0);break;}
            mode(w,1,0,5,0);e->flg|=0x2100000;
        }
        break;
    default:gap(w,"bhEne04_DG01 mode3");break;
    }
}

/* en04.c:491 bhEne04_DamageMode2[3] = { DG00, DG01, DG00 }. */
static void damage_state(RECVX_ENEMY_WORK* w) {
    switch(w->mode2) {
    case 0:dg00(w);break;case 1:dg01(w);break;case 2:dg00(w);break;
    default:gap(w,"bhEne04_DamageMode2 index");break;
    }
}

static void dd00(RECVX_ENEMY_WORK* w) { /* en04.c:3370 */
    RECVX_ENEMY_VEC pos;
    switch(w->mode3) {
    case 0:
        w->flg|=2;w->flg&=~0x28u;pos=position(w);
        if(context.en04&&context.en04->blood_pool_lnk)
            context.en04->blood_pool_lnk(w,&pos,w->ay,&recvx_en04_blood_pool,0);
        else gap(w,"bhSetBloodPoolLnk");
        w->mode3++;
        /* en04.c:3383: kein break */
    case 1:break;
    default:gap(w,"bhEne04_DD00 mode3");break;
    }
}

/* en04.c:498 bhEne04_DieMode2[1]. */
static void die_state(RECVX_ENEMY_WORK* w) {
    switch(w->mode2) {
    case 0:dd00(w);break;
    default:gap(w,"bhEne04_DieMode2 index");break;
    }
}

/* en04.c:1027/1034/1041: Nage/Damage/Die loeschen zuerst die Nackenfreigabe
   und laufen dann ueber ihre Type-Tabelle; alle drei Tabellen fuehren je
   Typ 0..2 denselben Einstieg (en04.c:431/438/445). */
void recvx_en04_nage(RECVX_ENEMY_WORK* w) {
    if(!w->exp0){gap(w,"EN04 exp0");return;}
    ex(w)->flg&=~0x80u;
    if(w->type>2){gap(w,"bhEne04_NageType index");return;}
    grab_state(w);
}
void recvx_en04_damage(RECVX_ENEMY_WORK* w) {
    if(!w->exp0){gap(w,"EN04 exp0");return;}
    ex(w)->flg&=~0x80u;
    if(w->type>2){gap(w,"bhEne04_DamageType index");return;}
    damage_state(w);
}
void recvx_en04_die(RECVX_ENEMY_WORK* w) {
    if(!w->exp0){gap(w,"EN04 exp0");return;}
    ex(w)->flg&=~0x80u;
    if(w->type>2){gap(w,"bhEne04_DieType index");return;}
    die_state(w);
}
