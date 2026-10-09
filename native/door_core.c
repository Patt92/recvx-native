/* Door demo (door.c semantics) as a native mechanism: the fade procedure, the
   seven camera procedures, the seven door procedures and the pass driver of
   bhControlDoor. The MPAC parameter blocks are data layouts of the packet
   format, so they are declared here; no original procedure is linked and no
   generator is involved.

   Not driven here: the light procedures (LightProc1/2, DokiProcLgt) and the
   PuruProc shake. Their procedure numbers are parsed and reported through
   RECVX_DOOR_STATE::lgt_type / pru_type instead of being dropped. */
#include "door_core.h"
#include "ninja_math.h"
#include <math.h>
#include <string.h>

/* ---- MPAC parameter blocks ------------------------------------------- */
typedef struct {float x,y,z;} DPOINT;
typedef struct { /* 0x34 */
    int wait0;unsigned color0;float rate0,speed0,goal0;int cmp0;
    int wait1;unsigned color1;float rate1,speed1,goal1;int cmp1;
    int wait2;
} FADE_PARAM;
typedef struct { /* 0x50, shared by view types 1 and 7 */
    int wait_push;DPOINT pos_push;int wait_pull;DPOINT pos_pull;int ang[3];
    int vew0_yaw,vew0_pitch;float vew0_speed,vew0_accel,vew0_goal_pz;
    int vew0_cmp,vew0_goal_dy,vew0_speed_dy,vew0_cmp_dy;
} VIEW1_PARAM;
typedef struct { /* half of view type 2 */
    DPOINT pos,tgt;int roll,yaw,pitch;float speed,accel;
} VIEW2_HALF;
typedef struct { /* 0x5C */
    VIEW2_HALF low,high;int wait;
} VIEW2_PARAM;
typedef struct { /* half of view type 3 */
    DPOINT pos;int ang[3],dst_ang[3];float ang_rte;int yaw,pitch;float speed,accel;
    int speed_dx,accel_dx,loop_wait;
} VIEW3_HALF;
typedef struct { /* 0x94 */
    VIEW3_HALF low,high;int pos_wait,ang_wait,slope_ax;
} VIEW3_PARAM;
typedef struct {DPOINT pos;int ang[3];} VIEW4_HALF;
typedef struct { /* 0x38 */
    VIEW4_HALF low,high;int wait,chg_vew;
} VIEW4_PARAM;
typedef struct { /* 0x5C */
    VIEW4_HALF low,high;
    int wait0;float fde_rte0,fde_spd0,fde_gol0;int fde_cmp0;
    int wait1;float fde_rte1,fde_spd1,fde_gol1;int fde_cmp1;
    int chg_vew;
} VIEW5_PARAM;
typedef struct { /* 0x58 */
    VIEW4_HALF low,high;int wait_pos,wait_ang;DPOINT pos_rte,ang_rte;
    int total_time,chg_vew;
} VIEW6_PARAM;
typedef struct { /* 0x50, shared by door types 1 and 7 */
    int nob_first,nob_speed,nob_accel,nob_goal,nob_cmp;
    int dor0_wait,dor0_first,dor0_speed,dor0_accel,dor0_goal_speed,dor0_cmp;
    int dor1_wait,dor1_speed,dor1_accel,dor1_goal_angle,dor1_cmp,dor1_decel;
    int wait,snd_wait,snd_wait1;
} DOOR1_PARAM;
typedef struct { /* 0x1C */
    int wait,snd_wait,yaw,pitch;float speed,decel,goal;
} DOOR2_PARAM;
typedef struct { /* 0x40 */
    int nob_wait,nob_first,nob_speed,nob_accel,nob_goal,nob_cmp;
    int dor0_wait,dor0_yaw,dor0_pitch;float dor0_speed,dor0_accel;int dor0_cmp;float dor0_goal;
    int wait,snd_wait,type;
} DOOR3_PARAM;
typedef struct {float pos_y;int yaw,pitch;} DOOR4_HALF;
typedef struct { /* 0x30 */
    DOOR4_HALF low,high;int wait,snd_wait;float speed,accel,goal;int cmp;
} DOOR4_PARAM;
typedef struct { /* 0x34 */
    int dor0_wait,dor0_first,dor0_speed,dor0_accel,dor0_goal_speed,dor0_cmp;
    int dor1_wait,dor1_speed,dor1_accel,dor1_goal_angle,dor1_cmp,dor1_decel;
    int snd_wait;
} DOOR5_PARAM;

/* ---- helpers ---------------------------------------------------------- */
static int compare_int(int a,int type,int b){
    switch(type){
    case -1:return a>=b;
    case 0:return a==b;
    case 1:return a<=b;
    case -2:case 2:return a!=b;
    default:return 0;
    }
}
static int compare_float(float a,int type,float b){
    switch(type){
    case -1:return a>=b;
    case 0:return a==b;
    case 1:return a<=b;
    case -2:case 2:return a!=b;
    default:return 0;
    }
}
/* VectorMove: the point travels `speed` along -Z of the yaw/pitch frame. */
static void vector_move(NJS_POINT3* position,int yaw,int pitch,float speed){
    NJS_MATRIX m;
    njUnitMatrix(&m);
    m[12]=position->x;m[13]=position->y;m[14]=position->z;
    njRotateY(&m,yaw);
    njRotateX(&m,pitch);
    njTranslate(&m,0,0,-speed);
    position->x=m[12];position->y=m[13];position->z=m[14];
}
static void move_float3(float* position,int yaw,int pitch,float speed){
    NJS_POINT3 p;p.x=position[0];p.y=position[1];p.z=position[2];
    vector_move(&p,yaw,pitch,speed);
    position[0]=p.x;position[1]=p.y;position[2]=p.z;
}
static void set_angles(int* destination,const int* source){
    destination[0]=source[0];destination[1]=source[1];destination[2]=source[2];
}
static void set_point(NJS_POINT3* destination,const DPOINT* source){
    destination->x=source->x;destination->y=source->y;destination->z=source->z;
}

/* ---- parameter sizes and the chg_vew chain ----------------------------- */
size_t recvx_door_parameter_size(unsigned family,unsigned type){
    static const size_t fade[2]={0,sizeof(FADE_PARAM)};
    static const size_t view[8]={0,sizeof(VIEW1_PARAM),sizeof(VIEW2_PARAM),sizeof(VIEW3_PARAM),
                                 sizeof(VIEW4_PARAM),sizeof(VIEW5_PARAM),sizeof(VIEW6_PARAM),sizeof(VIEW1_PARAM)};
    /* Door type 6 only raises an evalflag and carries no block. */
    static const size_t door[8]={0,sizeof(DOOR1_PARAM),sizeof(DOOR2_PARAM),sizeof(DOOR3_PARAM),
                                 sizeof(DOOR4_PARAM),sizeof(DOOR5_PARAM),0,sizeof(DOOR1_PARAM)};
    if(family==0)return type<2?fade[type]:0;
    if(family==1)return type<8?view[type]:0;
    if(family==2)return type<8?door[type]:0;
    return 0;
}
static int chained_view(unsigned type,const unsigned char* block,int* next){
    switch(type){
    case 4:*next=((const VIEW4_PARAM*)block)->chg_vew;return 1;
    case 5:*next=((const VIEW5_PARAM*)block)->chg_vew;return 1;
    case 6:*next=((const VIEW6_PARAM*)block)->chg_vew;return 1;
    default:return 0;
    }
}
int recvx_door_validate_view(unsigned type,const unsigned char* parameters,size_t bytes,unsigned depth){
    size_t size;int next=0;
    if(type==0||type>7)return 0;
    if(depth>8)return 0;                 /* a chg_vew ring would never end */
    size=recvx_door_parameter_size(1,type);
    if(!size||size>bytes||!parameters)return 0;
    if(!chained_view(type,parameters,&next))return 1;
    return recvx_door_validate_view((unsigned)next,parameters+size,bytes-size,depth+1);
}

/* A chg_vew switch replaces only the camera procedure; the block that follows
   the current one becomes its parameter block. */
static void set_view_proc(RECVX_DOOR_STATE* state,int type);

/* ---- fade -------------------------------------------------------------- */
static void fade_proc1(RECVX_DOOR_STATE* s){
    const FADE_PARAM* p=(const FADE_PARAM*)s->fpP;
    switch(s->fde_mode){
    case 0:
        s->fde_reg=p->wait0;
        s->fde_mode++;
        /* fall through */
    case 1:
        if(s->fde_reg-- <= 0){
            s->fde_col=(int)p->color0;s->fde_rate=p->rate0;
            s->status|=0x100;s->status|=0x200;
            s->fde_mode++;
        } else break;
        /* fall through */
    case 2:
        s->fde_rate+=p->speed0;
        if(compare_float(s->fde_rate,p->cmp0,p->goal0)){
            s->status&=~0x100;
            s->fde_reg=p->wait1;
            s->fde_mode++;
        }
        break;
    case 3:
        if(s->fde_reg-- <= 0){
            s->fde_col=(int)p->color1;s->fde_rate=p->rate1;
            s->status|=0x100;
            s->fde_mode++;
        } else break;
        /* fall through */
    case 4:
        s->fde_rate+=p->speed1;
        if(compare_float(s->fde_rate,p->cmp1,p->goal1)){
            s->status&=~0x100;s->status&=~0x200;s->status|=0x20;
            s->fde_reg=p->wait2;
            s->fde_mode++;
        }
        break;
    case 5:
        if(s->fde_reg-- <= 0){s->status|=0x40;s->fde_mode++;}
        break;
    default:break;
    }
}

/* ---- camera ------------------------------------------------------------ */
static void view_proc1(RECVX_DOOR_STATE* s){
    const VIEW1_PARAM* p=(const VIEW1_PARAM*)s->vpP;
    const int flip=(s->status&0x8000)?-1:1;
    switch(s->vew_mode){
    case 0:
        if(s->status&0x4000){set_point(&s->vew_pos,&p->pos_pull);s->vew_reg=p->wait_pull;}
        else {set_point(&s->vew_pos,&p->pos_push);s->vew_reg=p->wait_push;}
        set_angles(s->vew_ang,p->ang);
        s->vew_yaw=(s->status&0x1000)?0:p->vew0_yaw*flip;
        s->vew_pitch=p->vew0_pitch;
        s->vew_speed=p->vew0_speed;
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg-- <= 0){s->status|=0x8000000;s->vew_mode++;}
        else break;
        /* fall through */
    case 2:
        if(compare_float(s->vew_pos.z,p->vew0_cmp,p->vew0_goal_pz)){
            vector_move(&s->vew_pos,s->vew_yaw,s->vew_pitch,s->vew_speed);
            s->vew_speed+=p->vew0_accel;
            if(compare_int(s->vew_yaw,p->vew0_cmp_dy*flip,p->vew0_goal_dy*flip)){
                s->vew_yaw+=p->vew0_speed_dy*flip;
                break;
            }
        } else s->vew_mode++;
        /* fall through */
    default:break;
    }
}
static void view_proc2(RECVX_DOOR_STATE* s){
    const VIEW2_PARAM* all=(const VIEW2_PARAM*)s->vpP;
    const VIEW2_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    NJS_VECTOR v;float y;
    switch(s->vew_mode){
    case 0:
        s->vew_reg=all->wait;
        set_point(&s->vew_pos,&p->pos);
        s->vew_yaw=p->yaw;s->vew_pitch=p->pitch;s->vew_speed=p->speed;
        s->vew_ang[2]=p->roll;
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg-- <= 0){
            s->status|=0x10000;s->status|=0x200000;s->status|=0x8000000;
            s->vew_mode++;
        } else break;
        /* fall through */
    case 2:
        vector_move(&s->vew_pos,s->vew_yaw,s->vew_pitch,s->vew_speed);
        s->vew_speed+=p->accel;
        break;
    default:break;
    }
    v.x=p->tgt.x-s->vew_pos.x;v.y=p->tgt.y-s->vew_pos.y;v.z=p->tgt.z-s->vew_pos.z;
    y=v.y;
    s->vew_ang[0]=(int)(10430.381f*atan2f(y,njSqrt(v.x*v.x+v.z*v.z)));
    s->vew_ang[1]=(int)(10430.381f*atan2f(-v.x,-v.z));
}
static void view_proc3(RECVX_DOOR_STATE* s){
    const VIEW3_PARAM* all=(const VIEW3_PARAM*)s->vpP;
    const VIEW3_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    switch(s->vew_mode){
    case 0:
        s->vew_reg=all->pos_wait;s->vew_tmp=all->ang_wait;
        set_point(&s->vew_pos,&p->pos);
        set_angles(s->vew_ang,p->ang);
        s->vew_yaw=p->yaw;s->vew_pitch=p->pitch;
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg-- <= 0){
            s->vew_speed=p->speed;s->vew_ang_speed=p->speed_dx;s->vew_reg=p->loop_wait;
            s->vew_bak=s->vew_pos;
            s->status|=0x8000000;
            s->vew_mode++;
        } else break;
        /* fall through */
    case 2:
        if(s->vew_reg-- <= 0){
            vector_move(&s->vew_pos,s->vew_yaw,s->vew_pitch,s->vew_speed);
            s->vew_speed+=p->accel;
            s->vew_ang_speed+=p->accel_dx;
            s->vew_pitch+=s->vew_ang_speed;
            if(compare_float(s->vew_pos.y-s->vew_bak.y,1,
                             (s->vew_pos.z-s->vew_bak.z)*-tanf(0.0000958738f*(float)all->slope_ax))){
                s->vew_speed=p->speed;s->vew_ang_speed=p->speed_dx;s->vew_pitch=p->pitch;
                s->vew_reg=p->loop_wait;
                s->vew_bak=s->vew_pos;
                s->status|=0x10000;
            }
        }
        break;
    default:break;
    }
    if(s->vew_tmp<=0){
        int i;
        for(i=0;i<3;++i)
            s->vew_ang[i]=(int)((float)s->vew_ang[i]+p->ang_rte*(float)(p->dst_ang[i]-s->vew_ang[i]));
    } else s->vew_tmp--;
}
static void view_proc4(RECVX_DOOR_STATE* s){
    const VIEW4_PARAM* all=(const VIEW4_PARAM*)s->vpP;
    const VIEW4_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    switch(s->vew_mode){
    case 0:
        s->vew_reg=all->wait;
        set_point(&s->vew_pos,&p->pos);
        set_angles(s->vew_ang,p->ang);
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg-- <= 0){
            s->vew_mode=0;
            set_view_proc(s,all->chg_vew);
            s->vpP=(void*)((unsigned char*)s->vpP+sizeof(VIEW4_PARAM));
        }
        break;
    default:break;
    }
}
static void view_proc5(RECVX_DOOR_STATE* s){
    const VIEW5_PARAM* all=(const VIEW5_PARAM*)s->vpP;
    const VIEW4_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    switch(s->vew_mode){
    case 0:
        s->vew_reg=all->wait0;
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg>0){
            if(--s->vew_reg==0){s->status|=0x100;s->fde_rate=all->fde_rte0;}
        } else {
            s->fde_rate+=all->fde_spd0;
            if(compare_float(s->fde_rate,all->fde_cmp0,all->fde_gol0)){
                s->fde_rate=all->fde_rte1;
                s->vew_reg=all->wait1;
                s->vew_mode++;
                set_point(&s->vew_pos,&p->pos);
                set_angles(s->vew_ang,p->ang);
                if(s->dmo_atr&0x2)s->vew_pos.x*=-1.0f;
            }
        }
        break;
    case 2:
        if(s->vew_reg>0){s->vew_reg--;break;}
        s->fde_rate+=all->fde_spd1;
        if(compare_float(s->fde_rate,all->fde_cmp1,all->fde_gol1)){
            s->status&=~0x100;
            s->vew_mode=0;
            set_view_proc(s,all->chg_vew);
            s->vpP=(void*)((unsigned char*)s->vpP+sizeof(VIEW5_PARAM));
        }
        break;
    default:break;
    }
}
static void view_proc6(RECVX_DOOR_STATE* s){
    VIEW6_PARAM* all=(VIEW6_PARAM*)s->vpP;
    const VIEW4_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    float adjust;
    switch(s->vew_mode){
    case 0:
        s->vew_reg=all->wait_pos;s->vew_tmp=all->wait_ang;
        s->vew_adj_ang=0;s->vew_adj_pos=0;
        s->vew_adj_pos_add=0.6f*(1.0f/(float)(all->total_time-all->wait_pos));
        s->vew_adj_ang_add=0.6f*(1.0f/(float)(all->total_time-all->wait_ang));
        s->vew_mode++;
        /* fall through */
    case 1:
        if(s->vew_reg<=0){
            s->vew_adj_pos+=s->vew_adj_pos_add;
            adjust=s->vew_adj_pos;
            if(s->dmo_atr&0x2)s->vew_pos.x+=adjust*(all->pos_rte.x*(-p->pos.x-s->vew_pos.x));
            else s->vew_pos.x+=adjust*(all->pos_rte.x*(p->pos.x-s->vew_pos.x));
            s->vew_pos.y+=adjust*(all->pos_rte.y*(p->pos.y-s->vew_pos.y));
            s->vew_pos.z+=adjust*(all->pos_rte.z*(p->pos.z-s->vew_pos.z));
        } else s->vew_reg--;
        if(s->vew_tmp<=0){
            s->vew_adj_ang+=s->vew_adj_ang_add;
            adjust=s->vew_adj_ang;
            s->vew_ang[0]+=(int)(adjust*(all->ang_rte.x*(float)(short)(p->ang[0]-s->vew_ang[0])));
            if(s->dmo_atr&0x2)
                s->vew_ang[1]+=(int)(adjust*(all->ang_rte.y*(float)(short)-(p->ang[1]+s->vew_ang[1])));
            else
                s->vew_ang[1]+=(int)(adjust*(all->ang_rte.y*(float)(short)(p->ang[1]-s->vew_ang[1])));
            s->vew_ang[2]+=(int)(adjust*(all->ang_rte.z*(float)(short)(p->ang[2]-s->vew_ang[2])));
        } else s->vew_tmp--;
        if(all->total_time-- <= 0){
            s->vew_mode=0;
            set_view_proc(s,all->chg_vew);
            s->vpP=(void*)((unsigned char*)s->vpP+sizeof(VIEW6_PARAM));
        }
        break;
    default:break;
    }
}
static void view_proc7(RECVX_DOOR_STATE* s){
    const VIEW1_PARAM* p=(const VIEW1_PARAM*)s->vpP;
    const int flip=(s->status&0x8000)?-1:1;
    if(s->vew_mode==0){
        s->vew_reg=(s->status&0x4000)?p->wait_pull:p->wait_push;
        s->vew_yaw=(s->status&0x1000)?0:p->vew0_yaw*flip;
        s->vew_pitch=p->vew0_pitch;
        s->vew_speed=p->vew0_speed;
        s->vew_mode++;
    } else view_proc1(s);
}

/* ---- door leaves -------------------------------------------------------- */
static void door_proc1(RECVX_DOOR_STATE* s){
    const DOOR1_PARAM* p=(const DOOR1_PARAM*)s->dpP;
    RECVX_DOOR_NODE* nodes=s->objP;
    RECVX_DOOR_NODE* leaf;RECVX_DOOR_NODE* other;
    int flip_door=(s->status&0x800)?-1:1,flip_knob=1;
    if(s->status&0x2000){flip_door=-flip_door;flip_knob=-flip_knob;}
    leaf =(s->status&0x2000)?&nodes[3]:&nodes[1];
    other=(s->status&0x2000)?&nodes[1]:&nodes[3];
    switch(s->dor_mode){
    case 0:
        s->dor_reg=p->wait;
        s->dor_mode++;
        /* fall through */
    case 1:
        if(s->dor_reg-- <= 0){
            leaf[1].ang[2]=p->nob_first*flip_knob;
            s->dor_ang_speed=p->nob_speed*flip_knob;
            s->dor_snd=p->snd_wait;s->dor_reg=p->dor0_wait;s->dor_snd_exd=p->snd_wait1;
            s->status|=0x200000;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 2:
        if(compare_int(leaf[1].ang[2],p->nob_cmp*flip_knob,p->nob_goal*flip_knob)){
            leaf[1].ang[2]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->nob_accel*flip_knob;
            if(s->dor_snd-- == 0)s->status|=0x10000;
            break;
        } else s->dor_mode++;
        /* fall through */
    case 3:
        if(s->dor_reg-- <= 0){
            leaf->ang[1]=p->dor0_first*flip_door;
            s->dor_ang_speed=p->dor0_speed*flip_door;
            s->dor_reg=p->dor1_wait;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 4:
        if(compare_int(s->dor_ang_speed,p->dor0_cmp*flip_door,p->dor0_goal_speed*flip_door)){
            leaf->ang[1]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor0_accel*flip_door;
            if(s->dor_snd_exd-- == 0)s->status|=0x40000;
            if(s->dor_snd-- == 0)s->status|=0x10000;
            s->status|=0x1000000;
            break;
        } else s->dor_mode++;
        /* fall through */
    case 5:
        if(s->dor_reg-- <= 0){s->dor_ang_speed=p->dor1_speed*flip_door;s->dor_mode++;}
        else break;
        /* fall through */
    case 6:
        if(compare_int(leaf->ang[1],p->dor1_cmp*flip_door,p->dor1_goal_angle*flip_door)){
            leaf->ang[1]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor1_accel*flip_door;
        } else {
            s->dor_ang_speed+=p->dor1_decel*flip_door;
            leaf->ang[1]+=s->dor_ang_speed;
        }
        if(compare_int(s->dor_ang_speed,p->dor1_cmp*flip_door,0)){s->status|=0x2000000;s->dor_mode++;}
        /* fall through */
    default:break;
    }
    if(s->status&0x1000){
        other->ang[1]=-leaf->ang[1];
        other[1].ang[2]=-leaf[1].ang[2];
    }
}
static void door_proc2(RECVX_DOOR_STATE* s){
    const DOOR2_PARAM* p=(const DOOR2_PARAM*)s->dpP;
    RECVX_DOOR_NODE* base=&s->objP[2];
    int i,j;
    switch(s->dor_mode){
    case 0:
        s->dor_reg=p->wait;
        s->dor_yaw=p->yaw;s->dor_pitch=p->pitch;s->dor_speed=0;
        s->dor_flt=base->pos[0]/njSin(base[6].ang[2]);
        s->status|=0x200000;
        s->dor_mode++;
        {   /* every slat shares the mesh of the first one */
            RECVX_DOOR_NODE* source=&s->objP[3];
            RECVX_DOOR_NODE* node=source;
            for(i=0;i<7;++i,++node)
                for(j=0;j<6;++j,++node)node->model=source->model;
        }
        /* fall through */
    case 1:
        if(s->dor_reg-- <= 0){
            s->dor_speed=p->speed;
            s->status|=0x10000;s->status|=0x1000000;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 2:
        if(compare_float(base->pos[0],1,p->goal)){
            s->status|=0x2000000;
            s->dor_speed=0;
            s->dor_mode++;
        } else s->dor_speed+=p->decel;
        break;
    default:break;
    }
    for(j=0;j<7;++j,++base){
        int angle,pole;float x;
        move_float3(base->pos,s->dor_yaw,s->dor_pitch,s->dor_speed);
        angle=(int)(10430.381f*asinf(base->pos[0]/s->dor_flt));
        pole=-angle;
        x=-base->pos[0]/2.0f;
        ++base;base->pos[0]=x;base->ang[2]=angle;
        ++base;base->pos[0]=x;base->ang[2]=pole;
        ++base;base->pos[0]=x;base->ang[2]=angle;
        ++base;base->pos[0]=x;base->ang[2]=pole;
        ++base;base->pos[0]=x;base->ang[2]=angle;
        ++base;base->pos[0]=x;base->ang[2]=pole;
    }
}
static void door_proc3(RECVX_DOOR_STATE* s){
    const DOOR3_PARAM* p=(const DOOR3_PARAM*)s->dpP;
    RECVX_DOOR_NODE* nodes=s->objP;
    const int flip_knob=(s->status&0x2000)?-1:1;
    RECVX_DOOR_NODE* leaf =(s->status&0x2000)?&nodes[3]:&nodes[1];
    RECVX_DOOR_NODE* other=(s->status&0x2000)?&nodes[1]:&nodes[3];
    switch(s->dor_mode){
    case 0:
        s->dor_reg=p->wait;s->dor_snd=p->snd_wait;
        s->dor_mode++;
        /* fall through */
    case 1:
        if(s->dor_reg-- <= 0){
            leaf[1].ang[2]=p->nob_first*flip_knob;
            s->dor_ang_speed=p->nob_speed*flip_knob;
            s->dor_reg=p->nob_wait;s->dor_snd=p->snd_wait;
            s->status|=0x200000;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 2:
        if(compare_int(leaf[1].ang[2],p->nob_cmp*flip_knob,p->nob_goal*flip_knob)){
            leaf[1].ang[2]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->nob_accel*flip_knob;
            if(s->dor_snd-- == 0)s->status|=0x10000;
            break;
        } else {s->dor_reg=p->dor0_wait;s->dor_mode++;}
        /* fall through */
    case 3:
        if(s->dor_reg-- <= 0){
            s->dor_yaw=p->dor0_yaw*flip_knob;
            s->dor_pitch=p->dor0_pitch;
            s->dor_speed=p->dor0_speed;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 4:
        if(s->dor_snd-- == 0){s->status|=0x10000;s->status|=0x1000000;}
        if(compare_float(s->dor_speed,p->dor0_cmp,p->dor0_goal)){
            move_float3(leaf->pos,s->dor_yaw,s->dor_pitch,s->dor_speed);
            s->dor_speed+=p->dor0_accel;
            if(p->type==1)move_float3(leaf[1].pos,s->dor_yaw,s->dor_pitch,s->dor_speed);
        } else {s->status|=0x2000000;s->dor_mode++;}
        /* fall through */
    default:break;
    }
    if(s->status&0x1000){
        other->pos[0]=-leaf->pos[0];other->pos[1]=leaf->pos[1];
        other[1].pos[0]=-leaf[1].pos[0];other[1].pos[1]=leaf[1].pos[1];
        other[1].ang[2]=-leaf[1].ang[2];
    }
}
static void door_proc4(RECVX_DOOR_STATE* s){
    const DOOR4_PARAM* all=(const DOOR4_PARAM*)s->dpP;
    const DOOR4_HALF* p=(s->status&0x4000)?&all->high:&all->low;
    RECVX_DOOR_NODE* nodes=s->objP;
    switch(s->dor_mode){
    case 0:
        s->dor_reg=all->wait;s->dor_snd=all->snd_wait;
        s->dor_speed=all->speed;
        s->dor_yaw=p->yaw;s->dor_pitch=p->pitch;
        nodes[2].pos[1]=p->pos_y;
        s->status|=0x200000;
        nodes[3].evalflags|=0x8;
        s->dor_mode++;
        /* fall through */
    case 1:
        if(s->dor_reg-- > 0){
            if(s->dor_reg<3)nodes[2].pos[1]+=0.2f*njCos(s->dor_reg*16384);
            if(s->dor_reg==1)s->status|=0x10000;
            break;
        }
        s->dor_reg=6;
        s->dor_mode++;
        break;
    case 2:
        if(s->dor_reg-- <= 0)s->dor_mode++;
        else break;
        /* fall through */
    case 3:
        if(compare_float(s->dor_speed,all->cmp,all->goal)){
            move_float3(nodes[2].pos,s->dor_yaw,s->dor_pitch,s->dor_speed);
            /* The original advances the camera speed here, not the door's. */
            s->vew_speed+=all->accel;
        }
        break;
    default:break;
    }
}
static void door_proc5(RECVX_DOOR_STATE* s){
    const DOOR5_PARAM* p=(const DOOR5_PARAM*)s->dpP;
    RECVX_DOOR_NODE* nodes=s->objP;
    switch(s->dor_mode){
    case 0:
        s->dor_reg=p->dor0_wait;s->dor_snd=p->snd_wait;
        s->dor_mode++;
        /* fall through */
    case 1:
        if(s->dor_reg-- <= 0){
            nodes[1].ang[0]=0;
            s->dor_ang_speed=p->dor0_speed;
            s->dor_reg=p->dor1_wait;
            s->dor_mode++;
            s->status|=0x400000;
        } else break;
        /* fall through */
    case 2:
        if(compare_int(s->dor_ang_speed,p->dor0_cmp,p->dor0_goal_speed)){
            nodes[1].ang[0]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor0_accel;
            if(s->dor_snd-- == 0)s->status|=0x80000;
            s->status|=0x1000000;
            break;
        }
        s->dor_mode++;
        /* fall through */
    case 3:
        if(s->dor_reg-- <= 0){
            s->dor_ang_speed=p->dor1_speed;
            s->dor_mode++;
            s->status|=0x20000;
        } else break;
        /* fall through */
    case 4:
        if(compare_int(nodes[1].ang[0],p->dor1_cmp,p->dor1_goal_angle)){
            nodes[1].ang[0]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor1_accel;
        } else {
            s->dor_ang_speed+=p->dor1_decel;
            nodes[1].ang[0]+=s->dor_ang_speed;
        }
        if(compare_int(s->dor_ang_speed,p->dor1_cmp,0)){
            s->status|=0x2000000;
            s->dor_mode++;
            s->status|=0x40000;
        }
        /* fall through */
    default:break;
    }
}
static void door_proc6(RECVX_DOOR_STATE* s){
    if(s->dor_mode==0){
        if(s->status&0x4000)s->objP[3].evalflags|=0x8;
        else s->objP[2].evalflags|=0x8;
        s->dor_mode++;
    }
}
static void door_proc7(RECVX_DOOR_STATE* s){
    const DOOR1_PARAM* p=(const DOOR1_PARAM*)s->dpP;
    RECVX_DOOR_NODE* nodes=s->objP;
    const int flip_door=(s->status&0x800)?-1:1;
    const int knob=(s->status&0x400)?2:1;
    const int flip_knob=(s->status&0x400)?1:-1;
    RECVX_DOOR_NODE* leaf =(s->status&0x2000)?&nodes[3]:&nodes[1];
    RECVX_DOOR_NODE* other=(s->status&0x2000)?&nodes[1]:&nodes[3];
    switch(s->dor_mode){
    case 0:
        s->dor_reg=p->wait;
        s->dor_mode++;
        /* fall through */
    case 1:
        if(s->dor_reg-- <= 0){
            leaf[knob].ang[0]=p->nob_first*flip_knob;
            s->dor_ang_speed=p->nob_speed*flip_knob;
            s->dor_snd=p->snd_wait;s->dor_reg=p->dor0_wait;s->dor_snd_exd=p->snd_wait1;
            s->status|=0x200000;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 2:
        if(compare_int(leaf[knob].ang[0],p->nob_cmp*flip_knob,p->nob_goal*flip_knob)){
            leaf[knob].ang[0]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->nob_accel*flip_knob;
            if(s->dor_snd-- == 0)s->status|=0x10000;
            break;
        } else s->dor_mode++;
        /* fall through */
    case 3:
        if(s->dor_reg-- <= 0){
            leaf->ang[1]=p->dor0_first*flip_door;
            s->dor_ang_speed=p->dor0_speed*flip_door;
            s->dor_reg=p->dor1_wait;
            s->dor_mode++;
        } else break;
        /* fall through */
    case 4:
        if(compare_int(s->dor_ang_speed,p->dor0_cmp*flip_door,p->dor0_goal_speed*flip_door)){
            leaf->ang[1]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor0_accel*flip_door;
            if(s->dor_snd_exd-- == 0)s->status|=0x40000;
            if(s->dor_snd-- == 0)s->status|=0x10000;
            s->status|=0x1000000;
            break;
        } else s->dor_mode++;
        /* fall through */
    case 5:
        if(s->dor_reg-- <= 0){s->dor_ang_speed=p->dor1_speed*flip_door;s->dor_mode++;}
        else break;
        /* fall through */
    case 6:
        if(compare_int(leaf->ang[1],p->dor1_cmp*flip_door,p->dor1_goal_angle*flip_door)){
            leaf->ang[1]+=s->dor_ang_speed;
            s->dor_ang_speed+=p->dor1_accel*flip_door;
        } else {
            s->dor_ang_speed+=p->dor1_decel*flip_door;
            leaf->ang[1]+=s->dor_ang_speed;
        }
        if(compare_int(s->dor_ang_speed,p->dor1_cmp*flip_door,0)){s->status|=0x2000000;s->dor_mode++;}
        break;
    default:break;
    }
    if(s->status&0x1000){
        other->ang[1]=-leaf->ang[1];
        other[knob].ang[0]=-leaf[knob].ang[0];
    }
}

/* ---- dispatcher --------------------------------------------------------- */
typedef void (*DOOR_PROC)(RECVX_DOOR_STATE*);
static const DOOR_PROC fade_table[2]={0,fade_proc1};
static const DOOR_PROC view_table[8]={0,view_proc1,view_proc2,view_proc3,view_proc4,view_proc5,view_proc6,view_proc7};
static const DOOR_PROC door_table[8]={0,door_proc1,door_proc2,door_proc3,door_proc4,door_proc5,door_proc6,door_proc7};

static void set_view_proc(RECVX_DOOR_STATE* state,int type){
    state->vew_prcP=(type>=0&&type<8)?view_table[type]:0;
}
int recvx_door_configure(RECVX_DOOR_STATE* state,unsigned fade,unsigned view,unsigned door){
    if(!state)return 0;
    state->fde_prcP=fade<2?fade_table[fade]:0;
    state->vew_prcP=view<8?view_table[view]:0;
    state->dor_prcP=door<8?door_table[door]:0;
    /* Door type 6 has no block and type 0 no procedure at all; the fade and the
       camera must both resolve or the demo would never finish. */
    return state->fde_prcP!=0&&state->vew_prcP!=0&&(door==0||state->dor_prcP!=0);
}

void recvx_door_step(RECVX_DOOR_STATE* s){
    if(!s||(s->status&0x40))return;
    if(s->status&0x80)return;            /* bhControlDoor holds the demo */
    if((s->status&0x1)&&s->fde_prcP)s->fde_prcP(s);
    if((s->status&0x2)&&s->vew_prcP)s->vew_prcP(s);
    if((s->status&0x4)&&s->dor_prcP)s->dor_prcP(s);
    s->time++;
}
