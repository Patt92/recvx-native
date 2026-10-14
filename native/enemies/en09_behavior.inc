/* Eigenstaendige EN09-Zustaende auf demselben Work. Referenz en09.c:2957ff.
   In en09.c eingebunden, damit keine zweite Work-/Dienstinstanz entsteht. */
static int en09_player(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_PLAYER* plp){
    if(epw->sv&&epw->sv->player_get&&epw->sv->player_get(plp))return 1;
    en09_gap(epw,"bhEne09: Spielerbesitzer fehlt");return 0;
}
static RECVX_ENEMY_VEC en09_pos(const RECVX_ENEMY_WORK* epw){
    RECVX_ENEMY_VEC p={epw->px,epw->py,epw->pz};return p;
}
static float en09_distance(RECVX_ENEMY_VEC a,RECVX_ENEMY_VEC b){
    float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return njSqrt(x*x+y*y+z*z);
}
static int en09_frames(RECVX_ENEMY_WORK* epw){
    if(epw->sv&&epw->sv->motion_frames){unsigned n=epw->sv->motion_frames(epw,epw->mtn_no);if(n)return (int)n;}
    en09_gap(epw,"bhEne09: mnwP[mtn_no].frm_num fehlt");return 0;
}
static void en09_motion(RECVX_ENEMY_WORK* epw,unsigned m,int h){
    recvx_t3_chg_mtn(epw,m,0,h);en09_ext(epw)->state&=0x1fffffffu;
}
static void en09_arm_reset(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    for(unsigned j=0;j<4;++j)en09_services->node_pos(epw,7+j,0,e->arm_len[j]);
}
static int en09_search(RECVX_ENEMY_WORK* epw){
    RECVX_ENEMY_VEC p=en09_pos(epw);int32_t angle;
    if(!en09_services||!en09_services->search||!en09_services->search(epw,&p,epw->ay,0x471c,&angle)){
        en09_gap(epw,"bhSearchPlayer: Anbieter fehlt");return -1;}
    return angle;
}
/* en09.c:3002-3046, byte search clock; unsigned temp cannot be < 0. */
static void en09_search_clock(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if((e->search&31)<31){
        if(en09_search(epw)!=-1)e->search|=0x20;
        if(e->search&0x20)e->search=(e->search|0x40)&0xdf;
        else if((e->search&31)==30)e->search&=0xbf;
    }
    ++e->search;if((e->search&31)>=31)e->search&=0xe0;
    if(e->search&0x40){e->state|=0x1000;e->sight_hold=15;}
    else if(e->state&0x1000)e->sight_hold=(int8_t)((uint8_t)e->sight_hold-1u);
}
static int en09_line(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_VEC a,RECVX_ENEMY_VEC* b,RECVX_ENEMY_VEC* normal,RECVX_ENEMY_ATR* hit){
    if(!epw->sv||!epw->sv->line){en09_gap(epw,"bhCollisionCheckLine");return -1;}
    return epw->sv->line(1,&a,b,normal,0,0,hit,NULL);
}
/* en09.c:3607, arm corridor through wall and enemy capsules, ten samples. */
static int en09_arm_line(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_VEC a,RECVX_ENEMY_VEC b){
    const RECVX_ENEMY_SERVICES* s=epw->sv;
    if(!s||!s->wall_type||!s->enemy_count||!s->enemy_at||!s->sphere_capsule){en09_gap(epw,"bhEne09_AramLineCheck: Dienste");return -1;}
    float dx=(b.x-a.x)/10.f,dz=(b.z-a.z)/10.f;
    for(int k=0;k<10;++k){
        if(s->wall_type(&a,epw->flg,2,2))return 1;
        for(unsigned j=0;j<s->enemy_count();++j){RECVX_ENEMY_WORK* other=s->enemy_at(j);
            if(other&&other!=epw&&!(other->flg&2)&&(other->flg&1)&&s->sphere_capsule(&a,3,&other->watr.c1,&other->watr.c2,other->watr.r))return 1;}
        a.x+=dx;a.z+=dz;
    }
    return 0;
}
/* en09.c:3488-3604: exact attack predicates, including short-circuit RNG. */
static int en09_attack(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp,int mode){
    RECVX_EN09_EXT* e=en09_ext(epw);RECVX_ENEMY_VEC a=en09_pos(epw),b=plp->pos,n;
    int hit=en09_line(epw,a,&b,&n,NULL);if(hit<0)return 0;
    float dx=plp->pos.x-epw->px,dz=plp->pos.z-epw->pz,dist=njSqrt(dx*dx+dz*dz);
    if(mode<2)return e->ply_dist>17&&e->ply_dist<20&&!hit&&epw->flr_no==plp->flr_no&&
        (!mode?(epw->hp<75&&plp->hp<80):1)&&recvx_t3_ikou3(epw,&plp->pos,0x2000)==0;
    float matrix[16];
    if(mode==2){
        if(dist>=13||recvx_t3_ikou3(epw,&plp->pos,0x3000))return 0;
        if(!epw->sv->joint_matrix||!epw->sv->joint_matrix(epw,4,matrix)){en09_gap(epw,"bhEne09_AttackCheck: owP[4]");return 0;}
        a=en09_pos(epw);a.y=matrix[13];b=plp->pos;b.y+=(matrix[13]-epw->py);
        return en09_line(epw,a,&b,&n,NULL)==0;
    }
    if(((plp->stflg&0x400)&&recvx_t3_rand_mod(8)==1)||recvx_t3_rand_mod(32)==1){
        float h=plp->pos.y-epw->py;if(h<0||h>10||dist<=14||dist>=50)return 0;
        if(!epw->sv->joint_matrix||!epw->sv->joint_matrix(epw,6,matrix)){en09_gap(epw,"bhEne09_AttackCheck: owP[6]");return 0;}
        a=en09_pos(epw);b=plp->pos;a.y=b.y=matrix[13];
        return en09_line(epw,a,&b,&n,NULL)==0&&en09_arm_line(epw,a,b)==0;
    }
    return 0;
}
static void en09_transition(RECVX_ENEMY_WORK* epw,int mode){epw->mode1=0;epw->mode2=(uint8_t)mode;epw->mode3=0;}
/* en09.c:3426: modes 0/1 have Brain00, modes 2..11 original dummy brain. */
static void en09_brain00(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(((plp->flg&6)||(plp->stflg&0x80000000u))&&e->ply_dist<17&&recvx_t3_ikou3(epw,&plp->pos,0x3000)==0){en09_transition(epw,9);return;}
    if(!(plp->flg&6)&&!(plp->stflg&0x80000000u)&&e->ct_14<=0&&!plp->debug_attack_disabled){
        static const int modes[4]={13,12,7,6};
        for(int i=0;i<4;++i)if(en09_attack(epw,plp,i)){en09_transition(epw,modes[i]);return;}
    }
    if(epw->flr_no==plp->flr_no&&recvx_t3_ikou3(epw,&plp->pos,0x5800)&&((epw->flg&4)||e->ply_dist<15))en09_transition(epw,8);
}
/* en09.c:3245: nearest floor-type-2 jump trigger; original list order. */
static int en09_close_floor(RECVX_ENEMY_WORK* epw,unsigned type,unsigned id){
    const RECVX_ENEMY_EXT* x=recvx_enemy_ext();RECVX_EN09_EXT* e=en09_ext(epw);float nearest=0;
    if(type<2||type>3||!x||!x->flr_count||!x->flr_at){en09_gap(epw,"bhEne09_GetCloseEnemyAtari");return 0;}
    int count=x->flr_count();
    for(int i=0;i<count;++i){RECVX_ENEMY_ATR a;if(!x->flr_at((unsigned)i,&a))continue;
        if(!(a.flg&1)||a.type!=2||a.flr_no!=epw->flr_no||a.prm0!=epw->id||a.prm1!=type-2||a.prm3==(uint8_t)id)continue;
        RECVX_ENEMY_VEC pos={a.px+a.w/2,a.py,a.pz+a.d/2};float distance=en09_distance(pos,en09_pos(epw));
        if(nearest==0||nearest>distance){nearest=distance;e->route_atr=(unsigned)i+1;e->route_rec=a;e->route_rec_valid=1;e->trg2_x=pos.x;e->trg2_z=pos.z;}
    }
    if(nearest<=0){e->route_atr=0;e->route_rec_valid=0;return 0;}return 1;
}
/* en09.c:3048: same-floor route and original cross-floor target selection.
   The wall-vault/arm-travel helpers remain explicit gaps until ported below. */
static int en09_arm_length(RECVX_ENEMY_WORK*,float*,RECVX_ENEMY_VEC*,RECVX_ENEMY_ATR*);
static float en09_arm_length_up(RECVX_ENEMY_WORK*);
static int en09_jump(RECVX_ENEMY_WORK*,const RECVX_ENEMY_PLAYER*);
static int en09_other(RECVX_ENEMY_WORK* epw,float distance,int angle){
    if(!epw->sv->enemy_at||!epw->sv->enemy_count){en09_gap(epw,"EN09 OtherEnemyCheck: Tabelle fehlt");return -1;}
    for(unsigned i=0;i<epw->sv->enemy_count();++i){RECVX_ENEMY_WORK* p=epw->sv->enemy_at(i);if(!p||p==epw||!(p->flg&1)||(p->flg&2))continue;
        RECVX_ENEMY_VEC pos=en09_pos(p);if(!recvx_t3_ikou3(epw,&pos,angle)&&en09_distance(pos,en09_pos(epw))<distance)return 1;}
    return 0;
}
static void en09_brain(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    RECVX_EN09_EXT* e=en09_ext(epw);en09_search_clock(epw);
    RECVX_ENEMY_VEC from=en09_pos(epw),to=plp->pos,next;
    if(epw->flr_no==plp->flr_no&&!(plp->stflg&0x10)){
        if(e->state&0x1000){
            if(!en09_services->route){en09_gap(epw,"bhCheckRoute");return;}
            int rid=en09_services->route(&from,&to,&next);
            e->trg_x=rid==255?to.x:next.x;e->trg_z=rid==255?to.z:next.z;
            if((epw->mode2==0||epw->mode2==1)&&rid!=255){
                RECVX_ENEMY_VEC target={e->trg_x,0,e->trg_z};RECVX_ENEMY_ATR a;
                if(!recvx_t3_ikou3(epw,&target,0x2000)&&en09_services->floor_area(&from,epw->flr_no,(uint8_t)epw->id,5,&a)){
                    RECVX_ENEMY_VEC ahead={epw->px-(3+epw->ar)*njSin(epw->ay),epw->py,epw->pz-(3+epw->ar)*njCos(epw->ay)},normal;
                    if(en09_line(epw,from,&ahead,&normal,&a)>0){e->route_rec=a;e->route_rec_valid=1;e->route_atr=1;
                        if(a.type==0&&a.h>0&&a.h<=8){epw->axp=(int32_t)(10430.381f*atan2f(normal.x,normal.z));epw->ayp=recvx_en09_chk_diff_angle(epw->ay,epw->axp);en09_transition(epw,4);}}}
                if(e->ply_dist>30&&!recvx_t3_ikou3(epw,&to,0xe38)&&!en09_services->floor_area(&from,epw->flr_no,(uint8_t)epw->id,4,&a)&&en09_other(epw,50,0xaaa)==0){
                    RECVX_ENEMY_VEC normal;
                    if(en09_arm_length(epw,&e->arm_len_out,&normal,&a)&&!(a.attr&8)&&a.h==0&&a.type==0&&recvx_t3_angle_check(&normal,epw->ay,0x800))en09_transition(epw,5);
                }
            }
        }
        e->state&=~0x02000000u;
    }else{
        if(!(e->state&0x02000000u)){
            if(plp->stflg&0x10){if(plp->mode0==1&&en09_close_floor(epw,plp->mode2==14?2:3,255))e->state|=0x02000000u;}
            else {RECVX_ENEMY_ATR a;if(en09_services->floor_area(&to,plp->flr_no,(uint8_t)epw->id,epw->flr_no<plp->flr_no?2:3,&a)&&en09_close_floor(epw,a.prm1,a.prm3))e->state|=0x02000000u;}
        }
        if(e->state&0x02000000u){to.x=e->trg2_x;to.z=e->trg2_z;memcpy(&to.y,e->reserved_48,4);
            int rid=en09_services->route(&from,&to,&next);e->trg_x=rid==255?to.x:next.x;e->trg_z=rid==255?to.z:next.z;
            if(en09_jump(epw,plp))e->state&=~0x02000000u;
        }else {e->trg_x=to.x;e->trg_z=to.z;}
    }
    if(epw->mode2<2)en09_brain00(epw,plp);
}
/* en09.c:3662/3714: idle and walking. Position comes from the original
   foot-fix tables in SetMtn, not from an invented chase speed. */
static void en09_mv00(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){en09_motion(epw,1,15);int n=en09_frames(epw);if(!n)return;
        epw->frm_no=recvx_t3_rand_mod(n)*65536;epw->ct0=recvx_t3_rand_mod(128)+200;
        en09_arm_reset(epw);epw->mode1=epw->type==10?0:1;++epw->mode3;}
    if(epw->mode3==1){--epw->ct0;if((en09_search(epw)!=-1||(epw->flg&4)||epw->ct0<=0)&&epw->type!=10){epw->mode1=1;epw->mode2=1;epw->mode3=0;}}
    (void)e;
}
static void en09_mv01(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){en09_motion(epw,0,15);epw->flg|=0x40000;++epw->mode3;}
    if(epw->mode3==1){
        if(e->state&0x1000){RECVX_ENEMY_VEC to={e->trg_x,0,e->trg_z};recvx_t3_ikou(epw,&to,0x16c);return;}
        if(recvx_t3_check_dir_wall2(epw,0,6)){int hit=recvx_t3_check_side_wall(epw,6,0);
            epw->way=hit?hit*0x16c:(recvx_t3_rand_mod(2)?0x16c:-0x16c);++epw->mode3;}
    }else if(epw->mode3==2){if(!recvx_t3_check_dir_wall2(epw,0,6)){epw->mode3=1;return;}epw->ay+=epw->way;}
}
/* en09.c:4583: turn toward player over the original thirteen passes. */
static void en09_mv08(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    if(epw->mode3==0){en09_motion(epw,0,8);epw->flg&=~0x40000u;
        int a=(recvx_t3_niten_dir(epw->px,epw->pz,plp->pos.x,plp->pos.z)-epw->ay)&65535;
        epw->ayp=(a<=32768?a:a-65536)/12;epw->ct0=12;++epw->mode3;}
    if(epw->mode3==1){epw->ay+=epw->ayp;if(--epw->ct0<0){epw->mode1=1;epw->mode2=1;epw->mode3=0;}}
}
static void en09_mv09(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    if(epw->mode3==0){en09_motion(epw,1,10);epw->flg|=0x40000;
        int a=(recvx_t3_niten_dir(epw->px,epw->pz,plp->pos.x,plp->pos.z)-epw->ay)&65535;
        epw->ayp=a<=32768?a:a-65536;epw->ct0=10;++epw->mode3;}
    if(epw->mode3==1){if(--epw->ct0>=0)epw->ay+=epw->ayp/10;
        if(!(plp->flg&4)){if(en09_attack(epw,plp,1))en09_transition(epw,12);else{epw->mode1=1;epw->mode2=1;epw->mode3=0;}}}
}
/* en09.c:3771: recover from grounded motion 39/56. */
static void en09_mv03(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){en09_motion(epw,epw->mtn_no==53?56:39,0);epw->ct0=recvx_t3_rand_mod(60)+30;
        e->state=(e->state&~15u)|0x2011;epw->flg|=0x20;epw->flg2|=1;++epw->mode3;}
    if(epw->mode3==1){if(--epw->ct0<0||(e->state&0x08000000)){en09_motion(epw,epw->mtn_no==39?29:54,0);e->state&=~0x08000000u;epw->flg|=0x40000;++epw->mode3;}return;}
    if(epw->mode3==2){int f=epw->frm_no>>16;
        if((epw->mtn_no==29&&f==50)||(epw->mtn_no==54&&f==65))e->state&=~0x40u;
        int n=en09_frames(epw);if(!n)return;
        if((epw->mtn_no==29&&f==56)||(epw->mtn_no==54&&f==n-1)){epw->flg2&=~1u;e->state&=~0x2010u;epw->mode1=1;epw->mode2=1;epw->mode3=0;}}
}
static void en09_move_state(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp);
/* en09.c:7037: sweep previous tip -> current tip; original five spheres. */
static int en09_arm_hit(RECVX_ENEMY_WORK* epw){
    const RECVX_ENEMY_SERVICES* s=epw->sv;const RECVX_ENEMY_EXT* x=recvx_enemy_ext();
    float m[16];RECVX_ENEMY_VEC end,at,delta;RECVX_ENEMY_CAPSULE cap;RECVX_ENEMY_PLAYER plp;
    if(!s||!s->joint_matrix||!s->sphere_capsule||!x||!x->player_watr||!s->joint_matrix(epw,13,m)||!x->player_watr(&cap)||!en09_player(epw,&plp)){
        en09_gap(epw,"bhEne09_CollChkArm2: Besitzer fehlt");return -1;}
    if(!recvx_t3_calc_parts_pos(epw,m,&end,(const uint8_t*)en09_tables(epw)->tree[2],11,1))return -1;
    if(!s->joint_matrix(epw,13,m))return -1;
    at=(RECVX_ENEMY_VEC){m[12],m[13],m[14]};delta=(RECVX_ENEMY_VEC){(end.x-at.x)/5,(end.y-at.y)/5,(end.z-at.z)/5};
    for(int i=0;i<5;++i){
        if(s->sphere_capsule(&at,3,&cap.c1,&cap.c2,cap.r)){
            if(!(plp.flg&4)&&!(plp.stflg&0x80000000u)&&!plp.debug_attack_disabled){
                if(x->player_hit_set)x->player_hit_set(2,NULL,&at);
                RECVX_T3_LINK link={NULL,1};recvx_t3_hit_check_parts(link,&at);
                plp.flg|=4;plp.mode0=2;plp.mode2=2;plp.mode3=0;plp.hp-=10;
                int r=(recvx_t3_niten_dir(plp.pos.x,plp.pos.z,epw->px,epw->pz)-plp.ay)&65535;
                plp.mode1=r<=16384||r>49152?0:1;s->player_set(&plp);en09_se_play(epw,0x12307);recvx_t3_set_vibration(1);
            }
            return 1;
        }
        at.x+=delta.x;at.y+=delta.y;at.z+=delta.z;
    }
    if(!s->enemy_count||!s->enemy_at||!s->wall_type){en09_gap(epw,"bhEne09_CollChkArm2: Kollision");return -1;}
    for(unsigned i=0;i<s->enemy_count();++i){RECVX_ENEMY_WORK* other=s->enemy_at(i);
        if(other&&other!=epw&&!(other->flg&2)&&(other->flg&1)&&s->sphere_capsule(&end,3,&other->watr.c1,&other->watr.c2,other->watr.r))return 1;}
    return s->wall_type(&end,0,3,3)!=0;
}
static float en09_extra_float(RECVX_EN09_EXT* e,unsigned off){float f;memcpy(&f,(unsigned char*)e+off,4);return f;}
static void en09_extra_set(RECVX_EN09_EXT* e,unsigned off,float f){memcpy((unsigned char*)e+off,&f,4);}
static int en09_node_x(RECVX_ENEMY_WORK* epw,unsigned node,float* out){RECVX_ENEMY_VEC p;
    if(!en09_services->node(epw,node,&p,NULL)){en09_gap(epw,"bhEne09: objP Arm fehlt");return 0;}*out=p.x;return 1;}
static void en09_arm_add(RECVX_ENEMY_WORK* epw,float add){
    for(unsigned j=7;j<11;++j){float f;if(!en09_node_x(epw,j,&f))return;en09_services->node_pos(epw,j,0,f+add);}}
static void en09_arm_deltas(RECVX_ENEMY_WORK* epw){RECVX_EN09_EXT* e=en09_ext(epw);
    for(unsigned j=0;j<4;++j){float f;if(!en09_node_x(epw,7+j,&f))return;en09_extra_set(e,0x6c+4*j,f-e->arm_len[j]);}}
static void en09_arm_retract(RECVX_ENEMY_WORK* epw,int passes){RECVX_EN09_EXT* e=en09_ext(epw);
    for(unsigned j=0;j<4;++j){float f;if(!en09_node_x(epw,7+j,&f))return;en09_services->node_pos(epw,7+j,0,f-en09_extra_float(e,0x6c+4*j)/passes);}}
static int en09_player_root(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_VEC* out){float m[16];
    if(epw->sv->player_joint_matrix&&epw->sv->player_joint_matrix(0,m)){*out=(RECVX_ENEMY_VEC){m[12],m[13],m[14]};return 1;}
    en09_gap(epw,"bhEne09: plp->owP[0]");return 0;}
/* en09.c:4309: extend arm, swept hit, retract, original cooldown. */
static void en09_mv06(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    switch(epw->mode3){
    case 0:en09_motion(epw,16,5);epw->flg|=0x40000;epw->way=0x71c;epw->ct0=epw->ct2=0;e->state|=0x8000;en09_arm_reset(epw);++epw->mode3;
        /* original fallthrough */
    case 1:{RECVX_ENEMY_VEC root;if(!en09_player_root(epw,&root))return;
        int r=recvx_t3_ikou3(epw,&root,epw->way);if(r)epw->ay+=r;else epw->ay=recvx_t3_niten_dir(epw->px,epw->pz,root.x,root.z);
        if((epw->frm_no>>16)==16){e->arm_len_out=e->ply_dist-15;if(e->arm_len_out<0)e->arm_len_out=0;
            en09_extra_set(e,0x70,0);en09_extra_set(e,0x74,0);epw->ct0=epw->ct1=0;++epw->mode3;}break;}
    case 2:{int f=epw->frm_no>>16;if(f==19)epw->mtn_add=0;
        float step=10-1.2f*epw->ct1;if(step<0)step=.1f;float total=en09_extra_float(e,0x70)+step;
        en09_extra_set(e,0x74,step);en09_extra_set(e,0x70,total);
        if(e->arm_len_out<total){en09_extra_set(e,0x74,e->arm_len_out-(total-step));epw->mtn_add=65536;++epw->mode3;break;}
        ++epw->ct1;en09_arm_add(epw,step/4);
        if(f>18&&en09_arm_hit(epw)>0){en09_se_play(epw,0x12307);en09_arm_add(epw,-step/4);epw->mtn_add=65536;++epw->mode3;}break;}
    case 3:if((epw->frm_no>>16)==25){en09_arm_deltas(epw);epw->ct1=8;epw->ct0=0;++epw->mode3;}en09_arm_hit(epw);break;
    case 4:if(epw->ct0<epw->ct1)en09_arm_retract(epw,epw->ct1);++epw->ct0;
        {int n=en09_frames(epw);if(n&&(epw->frm_no>>16)==n-1){epw->flg|=0x100;e->ct_14=recvx_t3_rand_mod(15)+10;e->state&=~0x8000u;epw->mode1=1;epw->mode2=1;epw->mode3=0;}}break;
    }
}
/* en09.c:4470: short arm swing and directional player damage. */
static void en09_mv07(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_PLAYER plp){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){int r=(recvx_t3_niten_dir(epw->px,epw->pz,plp.pos.x,plp.pos.z)-epw->ay)&65535;
        en09_motion(epw,r>32768&&r<=0xe7ff?49:45,8);epw->ayp=(r<=32768?r:r-65536)/5;epw->flg|=0x40000;epw->way=0x600;epw->ct0=5;++epw->mode3;}
    if(epw->mode3!=1)return;
    if(--epw->ct0>=0)epw->ay+=epw->ayp;
    int f=epw->frm_no>>16;float m[16];RECVX_ENEMY_VEC pos;RECVX_ENEMY_CAPSULE cap;const RECVX_ENEMY_EXT* x=recvx_enemy_ext();
    if(!recvx_t3_calc_parts_pos(epw,m,&pos,(const uint8_t*)en09_tables(epw)->tree[2],10,1))return;
    if((epw->mtn_no==45&&f>=10&&f<16)||(epw->mtn_no==49&&f>12&&f<=16)){
        if(!x||!x->player_watr||!x->player_watr(&cap)){en09_gap(epw,"bhEne09_MV07: plp->watr");return;}
        if(epw->sv->sphere_capsule(&pos,3,&cap.c1,&cap.c2,cap.r)&&!(plp.flg&4)&&!(plp.stflg&0x80000000u)&&!plp.debug_attack_disabled){
            if(x->player_hit_set)x->player_hit_set(2,NULL,&pos);RECVX_T3_LINK link={NULL,1};recvx_t3_hit_check_parts(link,&pos);
            plp.flg|=4;plp.mode0=2;plp.mode2=1;plp.mode3=0;plp.hp-=20;
            int r=(plp.ay-epw->ay)&65535;plp.mode1=epw->mtn_no==45?(r>0x2000&&r<0xa000?0:1):(r<32768?1:0);
            epw->sv->player_set(&plp);recvx_t3_set_vibration(1);en09_se_play(epw,0x12307);}
    }
    int n=en09_frames(epw);if(n&&f==n-1){e->state&=~0x100u;e->ct_14=recvx_t3_rand_mod(15)+10;epw->mode1=1;epw->mode2=1;epw->mode3=0;}
}
/* en09.c:5671, grab entry uses the actual player capsule and wall gate. */
static int en09_nage_check(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_PLAYER* plp,float h){
    const RECVX_ENEMY_EXT* x=recvx_enemy_ext();float m[16];RECVX_ENEMY_VEC pos;RECVX_ENEMY_CAPSULE cap;
    if((plp->flg&6)||(plp->stflg&0x80000000u)||plp->hp<0||plp->debug_attack_disabled)return 0;
    if(!x||!x->player_watr||!x->player_watr(&cap)||!recvx_t3_calc_parts_pos(epw,m,&pos,(const uint8_t*)en09_tables(epw)->tree[2],10,1)){en09_gap(epw,"bhEne09_PlyNageCheck: Spieler/Gelenk");return 0;}
    if(!epw->sv->sphere_capsule(&pos,2,&cap.c1,&cap.c2,cap.r))return 0;
    RECVX_ENEMY_VEC a=en09_pos(epw),b=plp->pos,n;a.y=b.y=epw->py+h;
    int hit=en09_line(epw,a,&b,&n,NULL);if(hit<0)return 0;pos.y=plp->pos.y;
    if(!hit&&!epw->sv->wall_type(&pos,0,4,plp->cah+1))return 1; /* plp->ar 3.5, ah = cah+1 */
    if(x->player_hit_set)x->player_hit_set(2,NULL,&pos);RECVX_T3_LINK link={NULL,1};recvx_t3_hit_check_parts(link,&pos);
    plp->flg|=4;plp->mode0=2;plp->mode2=1;plp->mode3=0;plp->hp-=10;
    int r=(recvx_t3_niten_dir(plp->pos.x,plp->pos.z,epw->px,epw->pz)-plp->ay)&65535;plp->mode1=r<=16384||r>49152?0:1;
    epw->sv->player_set(plp);en09_se_play(epw,0x12307);recvx_t3_set_vibration(1);return 2;
}
/* en09.c:4845/4964: two original grab attempts. */
static void en09_mv_grab(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_PLAYER plp,int lethal){
    RECVX_EN09_EXT* e=en09_ext(epw);int f=epw->frm_no>>16;
    if(epw->mode3==0){if(!lethal&&epw->mtn_no==0&&(f<20||f>49))e->state|=0x10000000;
        en09_motion(epw,lethal?50:57,5);epw->ay=recvx_t3_niten_dir(epw->px,epw->pz,plp.pos.x,plp.pos.z);
        if(lethal)epw->flg&=~0x40u;else epw->flg|=0x40000;e->state|=0x100;++epw->mode3;}
    f=epw->frm_no>>16;
    if(epw->mode3==1){
        if(!lethal&&f>9)e->state&=~0x10000000u;
        if(f<(lethal?17:16)){RECVX_ENEMY_VEC root;if(!en09_player_root(epw,&root))return;
            int r=recvx_t3_ikou3(epw,&root,0x71c);if(r)epw->ay+=r;else epw->ay=recvx_t3_niten_dir(epw->px,epw->pz,root.x,root.z);}
        if(f==(lethal?16:15)){
            if(en09_nage_check(epw,&plp,lethal?15:0)==1){
                plp.mode0=lethal?6:4;plp.mode1=0;plp.mode2=lethal?0:1;plp.mode3=0;
                if(lethal)plp.flg&=~0x10u;plp.flg|=0x10004;
                if(!lethal){plp.hp-=30;if(plp.hp<0)plp.stflg|=0x40000;}
                epw->sv->player_set(&plp);epw->mode0=2;epw->mode1=0;epw->mode2=lethal?0:1;epw->mode3=0;e->state|=0x04400000;epw->flg2|=1;
                if(!recvx_t3_cdir_check(plp.ay,epw->ay))e->state|=0x400;else e->state&=~0x400u;
            }else {en09_motion(epw,lethal?51:69,0);if(lethal)epw->flg|=0x40;else epw->flg|=0x40000;epw->mode3=2;}}
    }else if(epw->mode3==2){int n=en09_frames(epw);if(n&&f==n-1){epw->mtn_add=0;epw->mode1=0;epw->mode2=0;epw->mode3=0;e->state&=~0x100u;}}
}
static void en09_mv_jump(RECVX_ENEMY_WORK*,int);
static void en09_mv05(RECVX_ENEMY_WORK*,const RECVX_ENEMY_PLAYER*);
static void en09_mv_vault(RECVX_ENEMY_WORK*,int);
static void en09_move_state(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    switch(epw->mode2){
    case 0:en09_mv00(epw);break;case 1:en09_mv01(epw);break;case 2:break;
    case 3:en09_mv03(epw);break;case 5:en09_mv05(epw,plp);break;case 4:en09_mv_vault(epw,0);break;case 14:en09_mv_vault(epw,1);break;case 10:en09_mv_jump(epw,0);break;case 11:en09_mv_jump(epw,1);break;case 6:en09_mv06(epw);break;case 7:en09_mv07(epw,*plp);break;
    case 8:en09_mv08(epw,plp);break;case 9:en09_mv09(epw,plp);break;
    case 12:en09_mv_grab(epw,*plp,0);break;case 13:en09_mv_grab(epw,*plp,1);break;
    default:en09_gap(epw,"bhEne09_MoveMode2 ausserhalb");break;
    }
}
static void en09_smoke(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_VEC* ofs,int rot);
/* en09.c:5214-5325: enemy holds the corresponding player clip until release. */
static void en09_nage_state(RECVX_ENEMY_WORK* epw){
    RECVX_ENEMY_PLAYER plp;if(!en09_player(epw,&plp))return;RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){if(!recvx_t3_cdir_check(plp.ay,epw->ay))e->state|=0x400;else e->state&=~0x400u;}
    int n=en09_frames(epw);if(!n)return;
    if(epw->mode2==0){if(epw->mode3==0&&(epw->frm_no>>16)==n-1){
        epw->mtn_add=0;epw->mode0=1;epw->mode1=epw->mode2=0;epw->mode3=1;e->state&=~0x600100u;epw->flg|=0x20;epw->flg2&=~1u;}return;}
    if(epw->mode2!=1){en09_gap(epw,"bhEne09_NageMode2 ausserhalb");return;}
    if(epw->mode3==0&&(epw->frm_no>>16)==n-1){en09_motion(epw,plp.hp<0?59:58,0);epw->flg|=0x40020;epw->flg2&=~1u;
        if(plp.hp<0){plp.mode0=6;plp.mode3=4;}else plp.mode3=2;epw->sv->player_set(&plp);++epw->mode3;
    }else if(epw->mode3==1&&(epw->frm_no>>16)==n-1){epw->mode0=1;epw->mode1=epw->mode2=epw->mode3=0;e->state&=~0x100u;}
}
/* en09.c:5487: nonlethal throw and its lethal continuation. */
static void en09_player_grab(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_PLAYER* plp){
    RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_EN09_DATA* d=en09_tables(epw);
    const RECVX_ENEMY_EXT* x=recvx_enemy_ext();
    if(plp->ply_id>=4){en09_gap(epw,"EN09 sys->ply_id ausserhalb");return;}
    unsigned base=(unsigned)d->ply_mtn_offset[plp->ply_id];int back=(e->state&0x400)!=0;int lethal=plp->mode2==0;
    if(plp->mode3==0){
        plp->flg=(plp->flg&~0x40000u)|0x10000;plp->exp1_flags=(plp->exp1_flags|0x1e0)&~4u;
        epw->wax=back?-32768:0;plp->mtn_no=base+(lethal?0:4)+(back?0:1);
        static const float offset0[8][3]={{.0102239996f,0,-20.71177101f},{-.0102260001f,0,-20.71356964f},{.0102239996f,0,-20.50097275f},{-.0102260001f,0,-20.31103706f},{.0102239996f,0,-20.71177101f},{-.0102260001f,0,-20.71356964f},{.0102239996f,0,-20.50097275f},{-.0102260001f,0,-20.31103706f}};
        static const float offset1[8][3]={{.6000000238f,0,.400000006f},{0,0,0},{.6999999881f,0,0},{0,0,0},{.6000000238f,0,.400000006f},{0,0,0},{.6999999881f,0,0},{0,0,0}};
        unsigned k=plp->ply_id*2+(back?0:1);memcpy(e->reserved_38,lethal?offset0[k]:offset1[k],12);
        plp->ay=(epw->ay+epw->wax)&65535;plp->mtn_enemy=1;plp->mtn_enemy_owner=epw->id;plp->mtn_enemy_work=epw;
        plp->frm_no=lethal?0x30000:0x20000;plp->hokan_count=0;plp->hokan_rate=0xc000;plp->mtn_add=65536;plp->mtn_md=0;plp->ct0=plp->ct1=0;
        if(!lethal)plp->flg2|=1;e->state|=lethal?0x200000:0x800000;plp->mode3=1;
        recvx_t3_player_se_play(epw,0x402);float m[16];if(epw->sv->player_joint_matrix&&epw->sv->player_joint_matrix(1,m)){
            RECVX_ENEMY_VEC p={m[12],epw->py+1,m[14]};for(int i=0;i<6;++i)en09_smoke(epw,&p,epw->ay+45056+i*8192);}
        if(!lethal){epw->sv->player_set(plp);return;}
    }
    int f=plp->frm_no>>16;
    if(lethal){
        if(plp->mode3==1){if(f==71)recvx_t3_player_se_play(epw,0x402);else if(f==105||f==114)en09_se_play(epw,0x230d);
            if(f==70)en09_gap(epw,"bhEne09_PlyDG00: Partikelrichtung pd ungeklaert (Originaldisassembly)");
            if(f==60)plp->flg|=2;
            unsigned n=epw->sv->player_motion_frames?epw->sv->player_motion_frames(plp->mtn_no,1):0;
            if(!n)en09_gap(epw,"EN09 Griff: Spieler-Motionquelle");
            else if(f==(int)n-1){plp->mtn_add=0;e->state&=~0x600000u;plp->mode3=2;plp->ct0=1;plp->ct1=115;}
        }else if(plp->mode3==2&&--plp->ct0<0){if(x&&x->pad_on_clear)x->pad_on_clear(15);plp->flg&=~0x10004u;plp->stflg=(plp->stflg&~0x10000u)|0x40000;plp->at_flg=0;plp->hp=-1;e->state&=~0x400000u;}
    }else switch(plp->mode3){
    case 1:break;
    case 2:plp->mtn_no=base+(back?6:7);plp->frm_no=0;plp->hokan_count=0;++plp->mode3;break;
    case 3:
        if(f>=20)e->state&=~0x800000u;
        if(f==0){if(x&&x->pad_on_clear)x->pad_on_clear(15);plp->flg=(plp->flg&~0x10004u)|8;plp->flg2&=~1u;plp->stflg&=~0x10000u;plp->at_flg=0;
            plp->mtn_enemy=0;plp->mtn_enemy_work=NULL;e->state&=~0x04400000u;plp->mode0=1;plp->mode1=plp->mode2=plp->mode3=0;}break;
    case 4:plp->mtn_no=base+(back?8:9);plp->frm_no=0;plp->hokan_count=0;plp->flg|=0x40000;plp->stflg|=0x40000;e->state&=~0x800000u;
        recvx_t3_player_se_play(epw,0x401);plp->hp=-1;++plp->mode3;
        /* original fallthrough */
    case 5:f=plp->frm_no>>16;
        if(plp->mtn_no==base+8&&f<5)plp->flg|=0x80000;else plp->flg&=~0x80000u;
        if((plp->mtn_no==base+8&&(f==47||f==51))||(plp->mtn_no!=base+8&&(f==30||f==51)))en09_se_play(epw,0x230d);
        {unsigned n=epw->sv->player_motion_frames?epw->sv->player_motion_frames(plp->mtn_no,1):0;
            if(n&&f==(int)n-1){plp->mtn_add=0;plp->flg|=2;plp->flg2&=~1u;}}break;
    }
    epw->sv->player_set(plp);
}
/* en09.c:2920: attachment offset is held in this enemy's exp0; neither a
   second player nor a synthetic pose. The hand path evaluates player's
   existing object tree through the owner before applying its chain offset. */
static void en09_player_link(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);if(!(e->state&0xa00000u))return;
    RECVX_ENEMY_PLAYER plp;if(!en09_player(epw,&plp))return;
    NJS_MATRIX m;NJS_POINT3 ofs,p;memcpy(&ofs,e->reserved_38,12);
    if(e->state&0x200000u){njUnitMatrix(&m);njTranslate(&m,epw->px,epw->py,epw->pz);njRotateXYZ(&m,epw->ax,epw->ay,epw->az);njCalcPoint(&m,&ofs,&p);}
    else {float joint[16];if(!epw->sv->joint_matrix(epw,13,joint)){en09_gap(epw,"bhEne09_PlayerLink: owP[13]");return;}
        memcpy(m,joint,sizeof m);njCalcPoint(&m,&ofs,&p);
        RECVX_ENEMY_VEC chain;
        if(!en09_services->player_chain||!en09_services->player_chain(&chain)){en09_gap(epw,"bhEne09_PlayerLink: Spielerbaum fehlt");return;}
        p.x+=plp.pos.x-chain.x;p.z+=plp.pos.z-chain.z;}
    plp.pos.x=p.x;plp.pos.z=p.z;plp.ay=(epw->ay+epw->wax)&65535;epw->sv->player_set(&plp);
}

static void en09_smoke(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_VEC* ofs,int rot){
    if(en09_services&&en09_services->smoke)en09_services->smoke(epw,ofs,rot);
    else en09_gap(epw,"bhEne09_SetSmokeEffect");
}
/* en09.c:2685: damage transition table, packed mode bytes on this Work. */
static int en09_ex_damage(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_EN09_DATA* d=en09_tables(epw);int f=epw->frm_no>>16,hit=0;
    for(unsigned i=0;i<RECVX_EN09_CHG_TBL_COUNT;++i){const RECVX_EN09_CHK_TBL* t=&d->chg_mtn_tbl[i];if(t->mtn_no==-1)break;
        if(t->mtn_no!=(int)epw->mtn_no||f<t->s_frm||f>t->e_frm)continue;
        unsigned k=(epw->hp<0?0:2)+((e->state&0x200)?0:1);const RECVX_EN09_CHG_TBL* c=&t->chg_tbl[k];
        if(c->mtn_no!=-1){recvx_t3_chg_mtn(epw,c->mtn_no,c->frm_no,c->h_cnt);e->state=c->type?((e->state&0x5fffffffu)|0x40000000u):(e->state&0x1fffffffu);}
        if(c->mode!=0xffffffffu){epw->mode0=(uint8_t)c->mode;epw->mode1=(uint8_t)(c->mode>>8);epw->mode2=(uint8_t)(c->mode>>16);epw->mode3=(uint8_t)(c->mode>>24);epw->ayp=epw->ay;}
        hit=1;break;
    }
    if(e->state&0x4000){epw->mode0=3;epw->mode1=0;epw->mode2=5;epw->mode3=0;return 1;}
    if(e->state&0x10000){epw->mode0=3;epw->mode1=0;epw->mode2=(e->state&0x200)?6:7;epw->mode3=0;epw->ayp=epw->ay;hit=1;}
    return hit;
}
static void en09_change_damage(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_EN09_DATA* d=en09_tables(epw);
    if(epw->wpnr_no<0||epw->wpnr_no>=22){en09_gap(epw,"En09_WpnDamageTbl index");return;}
    const RECVX_EN09_WPNDAMAGE* w=&d->wpn_damage[epw->wpnr_no];int action=w->nm_act;
    unsigned special=e->state&0xd6000u;
    if((w->flg&2)&&(epw->comb_flg&1)&&!special&&epw->hp>0&&recvx_t3_rand_mod(10)==0){action=2;epw->comb_flg&=~1u;en09_se_play(epw,0x0100230f);}
    if(epw->hp<0||(!(w->flg&8)&&special)){epw->comb_flg|=1;epw->comb_timeout=epw->comb_pnt=0;}
    if(epw->comb_flg&1){if(en09_ex_damage(epw))return;en09_se_play(epw,0x0100230f);action=w->cb_act;}else en09_se_play(epw,0x0100230e);
    if(action==0){if(!(e->state&0x100000)){e->state|=0x100000;epw->ct3=0;}}
    else if(action==5||action==1||action==2||action==3){epw->mode0=3;epw->mode1=epw->mode3=0;
        if(action==5)epw->mode2=0;else if(action!=3)epw->mode2=(uint8_t)action;
        else if(epw->comb_flg&4)epw->mode2=(w->flg&4)&&recvx_t3_rand_mod(3)==0?4:3;else epw->mode2=4;}
}
/* en09.c:2791: consume accumulated damage, blood and fire/acid requests. */
static void en09_damage_add(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_EN09_DATA* d=en09_tables(epw);const RECVX_ENEMY_EXT* x=recvx_enemy_ext();
    const RECVX_EN09_WPNDAMAGE* w=d->wpn_damage;RECVX_ENEMY_VEC ofs={0,0,0};
    if(!x||!x->player_way){en09_gap(epw,"EN09 plp->way");return;}
    if(epw->hp>=0){epw->hp-=epw->total_dam;
        if(epw->comb_wep>=22){en09_gap(epw,"EN09 comb_wep index");return;}w=&d->wpn_damage[epw->comb_wep];
        unsigned n=epw->sv->obj_num?epw->sv->obj_num(epw):0;if(n>64){en09_gap(epw,"EN09 obj_num > dam[64]");return;}
        for(unsigned i=1;i<n;++i){if(epw->dam[i]<=0)continue;epw->djnt_no=(int)i;if(w->flg&0x40)continue;
            int part;
            if((epw->comb_flg&1)||epw->hp<0){e->ct_1c=10;ofs.y=epw->djnt_no<18?1:-2;part=epw->djnt_no<18?3:1;
                recvx_t3_set_blood4(epw,part,&ofs,9,w->cb_blood==9?(epw->comb_flg&0x40?3:1):0);
                if(w->cb_blood==9)recvx_t3_set_blood2(epw,9,&ofs,x->player_way());
            }else if(w->nm_blood==9){if(!x||!x->player_way){en09_gap(epw,"EN09 plp->way");return;}
                recvx_t3_set_blood2(epw,9,&ofs,x->player_way());ofs.y=epw->djnt_no<18?1:-2;part=epw->djnt_no<18?3:1;
                recvx_t3_set_blood4(epw,part,&ofs,9,epw->comb_flg&0x40?3:0);
            }else recvx_t3_set_blood(epw,w->nm_blood&255,(const RECVX_T3_BT*)d->prt_blood_tbl);
        }
    }
    if((w->flg&0x30)&&e->ct_20<=0){e->ct_20=10;
        recvx_t3_set_dfire_effect(epw,epw->djnt_no,(const RECVX_T3_BT*)d->prt_blood_tbl,w->flg&0x20?2:1);
        if((w->flg&0x20)&&epw->hp<0){epw->mdflg|=0x400;if(en09_services->material)en09_services->material(epw,0xff000000);}}
    if((w->flg&0x80)&&e->ct_20<=0){e->ct_20=10;recvx_t3_set_san_effect(epw,epw->djnt_no,(const RECVX_T3_BT*)d->prt_blood_tbl);}
}
/* en09.c:2484, early ignored-melee branch confirmed at ELF 0x1c7a98. */
static void en09_damage_check(RECVX_ENEMY_WORK* epw){
    if(!(epw->flg&4)||(epw->flg&2))return;RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_EN09_DATA* d=en09_tables(epw);
    if(!e||!d)return;
    recvx_t3_calc_damage(epw,(const RECVX_T3_COMBWEP*)d->comb_wep,(const RECVX_T3_COMBJOINT*)d->comb_joint);
    if(!epw->total_dam)return;RECVX_ENEMY_PLAYER plp;if(!en09_player(epw,&plp))return;
    if((e->state&0x40)&&epw->flr_no<=plp.flr_no){uint32_t flags;
        if(!en09_services->weapon_flags||!en09_services->weapon_flags(epw->wpnr_no,&flags)){en09_gap(epw,"EN09 WpnTab");return;}
        if(!(flags&0x20000020u)&&(plp.at_flg&2))return;}
    en09_damage_add(epw);if(epw->mode0>=3){en09_se_play(epw,0x0100230e);return;}
    epw->flg|=0x40;e->state=(e->state&~0x10000000u)|0x08000000;
    if(epw->comb_flg&4)e->state|=0x200;else e->state&=~0x200u;en09_change_damage(epw);
    if(e->state&0x04000000){e->state&=0xfb1fffffu;plp.exp1_flags|=4;const RECVX_ENEMY_EXT* x=recvx_enemy_ext();if(x&&x->pad_on_clear)x->pad_on_clear(15);
        plp.flg=(plp.flg&~0x10000u)|8;plp.stflg&=0xfffaffffu;plp.at_flg=0;plp.mtn_enemy=0;plp.mtn_enemy_work=NULL;
        if(plp.flg2&0x200){plp.mode0=2;plp.mode2=1;plp.mode3=0;RECVX_ENEMY_VEC dv;
            if(!x||!x->player_hit_get||!x->player_hit_get(NULL,&dv,NULL)){en09_gap(epw,"EN09 Player damage direction");return;}
            plp.mode1=recvx_t3_dgc_dir_check(&dv,plp.ay)?1:0;plp.flg|=4;
        }else {plp.mode0=1;plp.mode1=plp.mode2=plp.mode3=0;plp.flg&=~4u;}epw->sv->player_set(&plp);}
}
/* en09.c:5752/5851/5955: standard stagger with six-pass arm restoration. */
static void en09_dg_small(RECVX_ENEMY_WORK* epw,int kind){
    RECVX_EN09_EXT* e=en09_ext(epw);RECVX_ENEMY_PLAYER plp;if(!en09_player(epw,&plp))return;
    if(epw->mode3==0){if(!epw->sv->grand){en09_gap(epw,"EN09 rom->grand");return;}
        epw->py=.001f+epw->sv->grand((unsigned)(epw->flr_no+2));epw->way=0x16c;epw->ct0=0;e->state&=~0x180u;epw->flg2&=~1u;
        int motion=(kind==0?36:kind==1?34:32)+((e->state&0x200)?1:0)+((e->state&0x8000)?30:0);
        en09_motion(epw,(unsigned)motion,5);epw->flg|=0x40000;en09_arm_deltas(epw);++epw->mode3;}
    if(epw->mode3==1){int f=epw->frm_no>>16,m=(int)epw->mtn_no;
        int n=en09_frames(epw);if(!n)return;
        int done=kind==0?f==5:kind==1?((m==35||m==65)&&f==9)||f==n-1:((m==33||m==63)?f>=26:f==22);
        if(done){e->state&=~0x8000u;epw->mode0=1;epw->mode3=0;
            if(epw->flr_no==plp.flr_no&&e->ply_dist>15&&recvx_t3_ikou3(epw,&plp.pos,0x6000)){epw->mode1=0;epw->mode2=8;}else{epw->mode1=1;epw->mode2=1;}}
        if(epw->ct0<6)en09_arm_retract(epw,6);else en09_arm_reset(epw);++epw->ct0;}
}
static void en09_damage_end(RECVX_ENEMY_WORK* epw){RECVX_EN09_EXT* e=en09_ext(epw);
    epw->mode0=epw->hp<0?4:1;epw->mode1=0;epw->mode2=epw->hp<0?0:3;epw->mode3=0;if(epw->hp>=0)e->state|=0x2040;}
/* en09.c:6198: backward knockdown. */
static void en09_dg04(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){e->state&=~0x1b0u;epw->flg&=~0x100u;epw->flg2&=~1u;epw->py=.001f+epw->sv->grand((unsigned)(epw->flr_no+2));
        int extended=(e->state&0x8000)!=0;en09_motion(epw,extended?60:53,5);
        if(extended)epw->flg|=0x40000;else e->state=(e->state&0x5fffffffu)|0x40000000;
        en09_arm_deltas(epw);epw->ct0=0;++epw->mode3;return;}
    if(epw->mode3==1){int n=en09_frames(epw);if(!n)return;int f=epw->frm_no>>16;
        if(f==n-1){epw->mtn_add=0;if(!epw->hokan_count){e->state=(e->state&0xffff3f70u)|1;en09_damage_end(epw);}}
        else {if(f==24)e->state=(e->state&~15u)|3;if(epw->ct0<6)en09_arm_retract(epw,6);else en09_arm_reset(epw);++epw->ct0;}}
}
/* en09.c:6924: original corpse pauses/restarts its end motion. */
static void en09_die_state(RECVX_ENEMY_WORK* epw){RECVX_EN09_EXT* e=en09_ext(epw);
    switch(epw->mode3){
    case 0:en09_motion(epw,(epw->mtn_no==53||epw->mtn_no==55)?56:39,0);epw->mtn_add=0;epw->flg=(epw->flg|2)&~0x28u;epw->ct0=1;epw->ct1=recvx_t3_rand_mod(5)+10;
        {RECVX_ENEMY_VEC p=en09_pos(epw);if(epw->sv->blood_pool)epw->sv->blood_pool(epw,&p,epw->ay,en09_tables(epw)->bld_tbl);else en09_gap(epw,"bhSetBloodPoolLnk");}
        ++epw->mode3; /* original fallthrough */
    case 1:if(epw->ct0--<=0){epw->mtn_add=65536;++epw->mode3;}break;
    case 2:{int n=en09_frames(epw);if(n&&(epw->frm_no>>16)==n-1){epw->mtn_add=0;if(epw->ct1-->0){epw->ct0=recvx_t3_rand_mod(50)+10;epw->mode3=1;}else ++epw->mode3;}}break;
    case 3:break;
    }
    (void)e;
}
static void en09_dg03(RECVX_ENEMY_WORK*);
static void en09_dg05(RECVX_ENEMY_WORK*);
static void en09_dg_air(RECVX_ENEMY_WORK*,int);
static void en09_dg10(RECVX_ENEMY_WORK*);
static void en09_damage_state(RECVX_ENEMY_WORK* epw){
    switch(epw->mode2){
    case 0:case 1:case 2:en09_dg_small(epw,epw->mode2);break;
    case 3:en09_dg03(epw);break;case 4:en09_dg04(epw);break;case 5:en09_dg05(epw);break;
    case 6:case 7:case 8:case 9:en09_dg_air(epw,epw->mode2);break;case 10:en09_dg10(epw);break;
    default:en09_gap(epw,"bhEne09_DamageMode2 ausserhalb");break;
    }
}

/* en09.c:7123/7222: temporary SetMotion on the SAME objP, then restore
   exactly the root, 24 angle triples and (in ChkArmLen) four arm positions.
   Other positions written by SetMotion remain, as in the original. */
typedef struct {uint32_t motion,hokan;int frame,rate,add;RECVX_ENEMY_VEC root;int32_t angles[24][3];float arms[4];} EN09_POSE_SAVE;
static int en09_save(RECVX_ENEMY_WORK* epw,EN09_POSE_SAVE* save){
    save->motion=epw->mtn_no;save->frame=epw->frm_no;save->hokan=epw->hokan_count;save->rate=epw->hokan_rate;save->add=epw->mtn_add;
    if(!en09_services->node(epw,0,&save->root,NULL))return 0;
    for(unsigned i=0;i<24;++i)if(!en09_services->node(epw,i,NULL,save->angles[i]))return 0;
    for(unsigned i=0;i<4;++i)if(!en09_node_x(epw,7+i,&save->arms[i]))return 0;return 1;
}
static void en09_restore(RECVX_ENEMY_WORK* epw,const EN09_POSE_SAVE* save,int arms){
    epw->mtn_no=save->motion;epw->frm_no=save->frame;epw->hokan_count=save->hokan;epw->hokan_rate=save->rate;epw->mtn_add=save->add;
    en09_services->node_pos(epw,0,0,save->root.x);en09_services->node_pos(epw,0,1,save->root.y);en09_services->node_pos(epw,0,2,save->root.z);
    for(unsigned i=0;i<24;++i)for(unsigned j=0;j<3;++j)en09_services->node_ang(epw,i,j,save->angles[i][j]);
    if(arms)for(unsigned i=0;i<4;++i)en09_services->node_pos(epw,7+i,0,save->arms[i]);
}
static int en09_arm_length(RECVX_ENEMY_WORK* epw,float* length,RECVX_ENEMY_VEC* normal,RECVX_ENEMY_ATR* hit){
    EN09_POSE_SAVE save;if(!en09_save(epw,&save)){en09_gap(epw,"bhEne09_ChkArmLen: 24 Knoten");return 0;}
    RECVX_ENEMY_VEC a,b;float m[16];int found=0;const uint8_t* tree=(const uint8_t*)en09_tables(epw)->tree[2];
    epw->mtn_no=7;epw->frm_no=0x120000;epw->hokan_count=0;epw->hokan_rate=0;epw->mtn_add=0;
    epw->sv->set_motion(epw,0,epw->mtn_md);
    if(!recvx_t3_calc_parts_pos(epw,m,&a,tree,11,1))goto restore;
    for(unsigned i=7;i<11;++i)en09_services->node_pos(epw,i,0,12.5f);
    if(!recvx_t3_calc_parts_pos(epw,m,&b,tree,11,1))goto restore;
    found=en09_line(epw,a,&b,normal,hit)>0;
    if(found){RECVX_ENEMY_VEC p={epw->px+epw->aox,epw->py,epw->pz+epw->aoz};float dx=(b.x-p.x)/10,dz=(b.z-p.z)/10;
        for(int i=0;i<10;++i){RECVX_ENEMY_ATR other;if(en09_services->wall(&p,epw->flg,epw->ar,epw->ah,&other)&&
            (other.px!=hit->px||other.py!=hit->py||other.pz!=hit->pz||other.type!=hit->type||other.id!=hit->id||other.w!=hit->w||other.h!=hit->h||other.d!=hit->d)){found=0;break;}p.x+=dx;p.z+=dz;}
        if(found){*length=en09_distance(a,b);if(*length<30)found=0;}}
    /* ELF 0x1d18ac writes uninitialized f20 on miss. No native value is
       invented: length/normal only valid with a hit; MV05 reports a miss. */
restore:
    en09_restore(epw,&save,1);return found;
}
static float en09_arm_length_up(RECVX_ENEMY_WORK* epw){
    EN09_POSE_SAVE save;RECVX_ENEMY_VEC a,b;NJS_MATRIX first,second;
    if(!en09_save(epw,&save)){en09_gap(epw,"bhEne09_ChkArmLen2: 24 Knoten");return 0;}
    const uint8_t* tree=(const uint8_t*)en09_tables(epw)->tree[2];epw->mtn_no=21;epw->frm_no=0;epw->hokan_count=0;epw->hokan_rate=0;epw->mtn_add=0;
    epw->sv->set_motion(epw,0,epw->mtn_md);recvx_t3_calc_parts_pos(epw,second,&a,tree,6,1);memcpy(first,second,sizeof first);
    recvx_t3_calc_parts_pos(epw,second,&b,tree+6,4,0);
    float oldy=b.y,oldz=b.z;b.x-=a.x;b.y-=a.y;b.z-=a.z;
    double height=(double)epw->sv->grand((unsigned)(epw->flr_no+3))-2.5;
    double t=(height-oldy)/b.y;
    second[12]=(float)((double)a.x+(double)b.x*t);second[13]=(float)height;second[14]=(float)((double)oldz+(double)b.z*t);
    njInvertMatrix(&first);njMultiMatrix(&first,&second);
    RECVX_EN09_EXT* e=en09_ext(epw);float result=first[12]-(e->arm_len[3]+(e->arm_len[2]+(e->arm_len[0]+e->arm_len[1])));
    en09_restore(epw,&save,0);return result;
}
/* en09.c:3309: cross-floor jump chosen by the original ATR list identity. */
static int en09_jump(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    RECVX_EN09_EXT* e=en09_ext(epw);const RECVX_ENEMY_EXT* x=recvx_enemy_ext();if(!e->route_rec_valid)return 0;
    if(!x||!x->flr_count||!x->flr_at){en09_gap(epw,"bhEne09_JumpCheck: flrp");return 0;}
    int count=x->flr_count();
    for(int i=0;i<count;++i){RECVX_ENEMY_ATR a;if(!x->flr_at((unsigned)i,&a))continue;
        if(!(a.flg&1)||a.type!=2||a.flr_no!=epw->flr_no||epw->px<a.px||epw->px>a.px+a.w||epw->pz<a.pz||epw->pz>a.pz+a.d||(unsigned)i+1!=e->route_atr)continue;
        if(a.prm2>=4){en09_gap(epw,"EN09 jump_ang[prm2]");return 0;}
        RECVX_CB_LINE line={a.px,a.py,a.pz,0,0,0};
        if(!(a.prm2&1)){line.pz+=a.d/2;line.vx=a.w;}else {line.px+=a.w/2;line.vz=a.d;}
        RECVX_CB_VEC point={epw->px,epw->py,epw->pz},foot;
        if(recvx_cb_distance_p2l(&point,&line,&foot)>=2)return 0;
        if(a.prm1==0){if(plp->pos.y>epw->py&&plp->mode0==1&&plp->mode2==15)return 0;en09_transition(epw,10);}
        else if(a.prm1==1){if(plp->pos.y<epw->py&&plp->mode0==1&&plp->mode2==14)return 0;en09_transition(epw,11);}else return 0;
        static const int angle[4]={0,49152,32768,16384};e->trg_x=foot.x;e->trg_z=foot.z;en09_extra_set(e,0x30,epw->py);epw->ayp=recvx_en09_chk_diff_angle(epw->ay,angle[a.prm2]);return 1;
    }
    return 0;
}
static void en09_speed(RECVX_ENEMY_WORK* epw,int offset){int a=(epw->ay+offset)&65535;epw->px-=epw->spd*njSin(a);epw->pz-=epw->spd*njCos(a);}
/* en09.c:4669/4758: upward/downward jump tables differ, including frame gates. */
static void en09_mv_jump(RECVX_ENEMY_WORK* epw,int down){
    RECVX_EN09_EXT* e=en09_ext(epw);int f=epw->frm_no>>16;
    if(epw->mode3==0){if(!down&&epw->type==9)epw->type=0;en09_motion(epw,down?44:43,5);e->state|=0x80000000u;
        epw->flg2|=1;epw->way=0x16c;epw->ct0=0;e->state|=down?0x800b0:0x480b0;epw->px=e->trg_x;epw->pz=e->trg_z;++epw->mode3;}
    if(epw->mode3==1){
        if(epw->flg&0x02000000){en09_motion(epw,1,10);epw->mtn_md|=0x100;epw->py=epw->sv->grand((unsigned)(epw->flr_no+2));epw->flg2&=~1u;
            e->state&=down?~0x800b0u:~0x400b0u;epw->ct0=10;++epw->mode3;return;}
        f=epw->frm_no>>16;if(f<5)epw->ay+=epw->ayp/5;
        const RECVX_EN09_ARM_POS* table=down?en09_tables(epw)->arm_pos2:en09_tables(epw)->arm_pos;
        if(f<0||(unsigned)f>=(down?RECVX_EN09_ARM_POS2_COUNT:RECVX_EN09_ARM_POS_COUNT)){en09_gap(epw,"EN09 jump arm table index");return;}
        for(unsigned j=7;j<11;++j){float value,last;if(!en09_node_x(epw,j,&value)||!en09_node_x(epw,10,&last))return;en09_services->node_pos(epw,j,0,value+(table[f].len-last)/4);}
        if(!down&&f==69){epw->flr_no=(int8_t)epw->sv->floor_num(epw->py);e->state&=~0x8000u;}
        if(down&&f==30)e->state&=~0x8000u;else if(down&&f==45)epw->flr_no=(int8_t)epw->sv->floor_num(epw->py);
    }else if(epw->mode3==2&&--epw->ct0<0){epw->mode1=1;epw->mode2=1;epw->mode3=0;epw->mtn_md&=~0x100u;}
}
/* en09.c:3853/5064: wall vault / fixed-distance leap share the same phases. */
static void en09_mv_vault(RECVX_ENEMY_WORK* epw,int fixed){
    RECVX_EN09_EXT* e=en09_ext(epw);int f=epw->frm_no>>16;
    switch(epw->mode3){
    case 0:en09_motion(epw,6,5);epw->flg|=0x40000;epw->way=0x16c;epw->ct0=0;en09_arm_reset(epw);
        e->arm_len_out=fixed?13:en09_arm_length_up(epw);e->state|=0x8030;en09_extra_set(e,0x60,epw->py);epw->flg2|=1;++epw->mode3;
        /* original fallthrough */
    case 1:f=epw->frm_no>>16;if(f<5)epw->ay+=epw->ayp/5;else if(f>=15&&f<19)en09_arm_add(epw,(e->arm_len_out/4)/4);
        if(f==18){epw->frm_no=0;epw->mtn_no=21;++epw->mode3;}break;
    case 2:if(epw->flg&0x02000000){epw->frm_no=0;epw->mtn_no=9;e->state|=0x4000;if(fixed){epw->flg&=~0x100u;epw->aoy=2;}else e->state|=0x80;
        en09_arm_add(epw,-(e->arm_len_out/4)/7);++epw->mode3;}break;
    case 3:if(!(epw->flg&0x02000000)){en09_arm_add(epw,-(e->arm_len_out/4)/7);return;}
        epw->frm_no=0;epw->mtn_no=12;epw->ct0=0;e->state&=~0x8000u;
        if(fixed)epw->spd=1.4f;else {if(!e->route_rec_valid){en09_gap(epw,"EN09 vault route ATR");return;}RECVX_ENEMY_ATR* a=&e->route_rec;int angle=epw->axp&65535;float len;
            if(angle>0x2000&&angle<=0x5fff)len=-(a->px-9-epw->px);
            else if(angle>0x6000&&angle<=0x9fff)len=9+a->pz+a->d-epw->pz;
            else if(angle>0xa000&&angle<=0xdfff)len=9+a->px+a->w-epw->px;
            else len=-(a->pz-9-epw->pz);int n=en09_frames(epw);if(!n)return;epw->spd=len/n;}
        ++epw->mode3;break;
    case 4:epw->py+=-.15f*epw->ct0;{float start=en09_extra_float(e,0x60);if(epw->py<start-.414835f)epw->py=start-.4f;}
        en09_speed(epw,0);++epw->ct0;if(epw->flg&0x02000000){e->state&=~0x40b0u;epw->flg2&=~1u;
            if(fixed){epw->flg|=0x100;epw->aoy=0;}epw->mtn_no=52;epw->frm_no=0;epw->ct0=0;epw->py=en09_extra_float(e,0x60);++epw->mode3;}break;
    case 5:{int n=en09_frames(epw);if(n&&(epw->frm_no>>16)==n-1){epw->mode1=1;epw->mode2=1;epw->mode3=0;}}break;
    }
}
/* en09.c:4026: arm attaches to the wall, pulls body and retreats. */
static void en09_mv05(RECVX_ENEMY_WORK* epw,const RECVX_ENEMY_PLAYER* plp){
    RECVX_EN09_EXT* e=en09_ext(epw);int f=epw->frm_no>>16;
    switch(epw->mode3){
    case 0:{en09_motion(epw,7,5);epw->flg|=0x40000;epw->flg2|=1;epw->way=0x5b0;epw->ct0=0;e->state|=0x8030;en09_arm_reset(epw);
        RECVX_ENEMY_VEC normal;RECVX_ENEMY_ATR hit;
        if(!en09_arm_length(epw,&e->arm_len_out,&normal,&hit)){en09_gap(epw,"bhEne09_MV05: ChkArmLen ohne gueltiges Ergebnis");return;}
        e->arm_len_out-=3;en09_extra_set(e,0x70,0);en09_extra_set(e,0x74,0);epw->ct0=1;epw->ct1=0;en09_extra_set(e,0x60,epw->py);
        epw->axp=(int32_t)(10430.381f*atan2f(normal.x,normal.z));epw->ayp=recvx_en09_chk_diff_angle(epw->ay,epw->axp);++epw->mode3;}
        /* original fallthrough */
    case 1:{int n=en09_frames(epw);if(n&&(epw->frm_no>>16)==n-3)++epw->mode3;}break;
    case 2:{int n=en09_frames(epw);if(!n)return;if((epw->frm_no>>16)==n-1)epw->mtn_add=0;
        float step=10-1.2f*epw->ct1;if(step<0)step=.1f;en09_extra_set(e,0x74,step);en09_extra_set(e,0x70,en09_extra_float(e,0x70)+step);
        en09_arm_add(epw,step/4);++epw->ct1;
        float m[16];RECVX_ENEMY_VEC a,b,hit,normal;if(!epw->sv->joint_matrix(epw,13,m)){en09_gap(epw,"EN09 MV05 owP[13]");return;}
        a=(RECVX_ENEMY_VEC){m[12],m[13],m[14]};if(!recvx_t3_calc_parts_pos(epw,m,&b,(const uint8_t*)en09_tables(epw)->tree[2],11,1))return;hit=b;
        if(en09_line(epw,a,&hit,&normal,NULL)>0){en09_arm_add(epw,-step/4);float distance=en09_distance(b,hit);step-=distance;en09_extra_set(e,0x74,step);en09_arm_add(epw,step/4);
            epw->mtn_no=19;epw->frm_no=0;epw->mtn_add=65536;++epw->mode3;}break;}
    case 3:if(epw->flg&0x02000000){epw->mtn_no=10;epw->frm_no=0;epw->mtn_add=65536;e->state|=0x10080;epw->flg&=~0x100u;
        en09_arm_deltas(epw);epw->ct1=6;epw->ct0=0;++epw->mode3;}break;
    case 4:{int n=en09_frames(epw);if(!n)return;f=epw->frm_no>>16;epw->ay+=epw->ayp/epw->ct1;
        if(f==n-1&&epw->mtn_no==10)epw->mtn_add=0;if(epw->ct0<epw->ct1)en09_arm_retract(epw,epw->ct1);++epw->ct0;
        if(epw->ct0>=epw->ct1&&f==n-1){epw->mtn_no=14;epw->frm_no=0;epw->hokan_count=0;epw->hokan_rate=0xcccc;epw->mtn_add=65536;epw->flg|=0x100;e->state&=~0x8000u;++epw->mode3;}break;}
    case 5:{int n=en09_frames(epw);if(!n)return;
        if((epw->frm_no>>16)==n-1&&(e->state&0x20000)){epw->mode0=3;epw->mode1=epw->mode2=epw->mode3=0;}
        if(epw->flg&0x02000000){epw->mtn_no=15;epw->frm_no=0;epw->hokan_count=0;epw->hokan_rate=0xcccc;epw->mtn_add=65536;epw->py=en09_extra_float(e,0x60);epw->ct0=0;
            float m[16];if(!epw->sv->joint_matrix(epw,0,m)){en09_gap(epw,"EN09 MV05 owP[0]");return;}epw->px=m[12];epw->py=m[13];epw->pz=m[14];
            float step=((en09_extra_float(e,0x60)-.143332f)-epw->py)/12;en09_extra_set(e,0x5c,step);epw->spd=.3f;epw->py+=step;++epw->mode3;}break;}
    case 6:{int n=en09_frames(epw);if(!n)return;f=epw->frm_no>>16;
        if(f==n-1){epw->flg|=0x100;epw->py=en09_extra_float(e,0x60);e->state&=~0x30u;epw->flg2&=~1u;epw->mode1=1;epw->mode2=1;epw->mode3=0;}
        else if(f<12){en09_extra_set(e,0x58,1);epw->spd=njCos(0x3000);en09_speed(epw,32768);epw->py+=njSin(0x3000)-epw->ct0*.15f;
            float floor=en09_extra_float(e,0x60)-.143332f;if(epw->py<floor)epw->py=floor;
            recvx_t3_ikou(epw,&plp->pos,epw->way);++epw->ct0;epw->spd=.3f;epw->py+=en09_extra_float(e,0x5c);
        }else {e->state&=~0x10080u;epw->py=en09_extra_float(e,0x60);}break;}
    case 7:break;
    }
}
/* en09.c:6985: side contact with four arm segments, y flattened at epw->py. */
static int en09_arm_contact(RECVX_ENEMY_WORK* epw,RECVX_ENEMY_VEC* position,float radius){
    for(unsigned j=6;j<10;++j){float a[16],b[16];if(!epw->sv->joint_matrix(epw,j,a)||!epw->sv->joint_matrix(epw,j+1,b)){en09_gap(epw,"EN09 arm owP[6..10]");return 0;}
        RECVX_ENEMY_VEC p1={a[12],epw->py,a[14]},p2={b[12],epw->py,b[14]};
        if(epw->sv->sphere_capsule(position,radius,&p1,&p2,1)==1){
            RECVX_CB_LINE line={p1.x,epw->py,p1.z,p2.x-p1.x,0,p2.z-p1.z};RECVX_CB_VEC p={position->x,position->y,position->z},foot;
            if(recvx_cb_distance_p2l(&p,&line,&foot)<radius+1){int angle=(int)(10430.381f*atan2f(foot.x-position->x,foot.z-position->z));
                position->x=foot.x-(radius+1)*njSin(angle);position->z=foot.z-(radius+1)*njCos(angle);}return 1;}
    }
    return 0;
}
/* en09.c:6060: forward knockdown, motion-specific root curve after state. */
static void en09_dg03(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);int f=epw->frm_no>>16;
    if(epw->mode3==0){e->state=(e->state&~0x1bfu)|3;epw->flg&=~0x120u;epw->flg2&=~1u;
        epw->py=.001f+epw->sv->grand((unsigned)(epw->flr_no+2));en09_motion(epw,e->state&0x8000?61:23,10);
        en09_arm_deltas(epw);epw->ayp=(int)(10430.381f*atan2f(epw->dvx,epw->dvz));f=epw->frm_no>>16;epw->ct0=0;++epw->mode3;
    }else if(epw->mode3==1){f=epw->frm_no>>16;if(f<10){int a=recvx_en09_chk_diff_angle(epw->ay,epw->ayp);epw->ay+=a>>1;}
        int n=en09_frames(epw);if(!n)return;
        if(f==n-1){epw->mtn_add=0;if(!epw->hokan_count){e->state=(e->state&0xffff3f70u)|1;en09_damage_end(epw);}}
        else {if(epw->ct0<6)en09_arm_retract(epw,6);else en09_arm_reset(epw);if(epw->ct0<5)epw->ay+=epw->ayp/5;++epw->ct0;}}
    if(f<0||f>=35){en09_gap(epw,"EN09 en09_mtn23[frm] outside (original table overread)");return;}
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,epw->px,epw->py,epw->pz);njRotateY(&m,epw->ay);
    NJS_POINT3 local={0,0,en09_tables(epw)->mtn23[f]},world;njCalcPoint(&m,&local,&world);epw->px=world.x;epw->py=world.y;epw->pz=world.z;
}
/* en09.c:6479/6574/6654/6758: air fall variants, no replacement gravity. */
static void en09_dg_air(RECVX_ENEMY_WORK* epw,int kind){
    RECVX_EN09_EXT* e=en09_ext(epw);int initial=epw->mode3==0;
    if(initial){e->state&=~0x1b0u;epw->flg2&=~1u;epw->ct0=0;
        if(kind==7){epw->flg|=0x100;en09_motion(epw,25,8);e->state&=0xfffe3f7fu;epw->py=.001f+epw->sv->grand((unsigned)(epw->flr_no+2));}
        else {epw->flg&=~0x100u;en09_motion(epw,61,8);e->state=(e->state&0x1fffffffu)|0x20000000;
            en09_extra_set(e,0x58,kind==6?1:kind==8?1.5f:.8f);
            if(kind==8){epw->flg2|=1;e->state=(e->state&~0x800fu)|6;}
            if(kind==9){epw->flg2|=1;e->state=(e->state&~15u)|2;RECVX_ENEMY_VEC pos=en09_pos(epw);epw->flr_no=(int8_t)epw->sv->floor_num(epw->sv->ground(&pos));}}
        en09_arm_deltas(epw);++epw->mode3;}
    if(epw->mode3!=1)return;
    float floor=epw->sv->grand((unsigned)(epw->flr_no+2));
    if(kind!=7){int angle=kind==9?0x2800:0x2000;float v=en09_extra_float(e,0x58);
        epw->py+=fabsf(v)*njSin(angle)-epw->ct0*.23f;
        if(epw->py<floor)epw->py=.001f+floor;
        else {epw->spd=v*njCos(angle);if(kind!=9)en09_speed(epw,32768);}}
    if(epw->ct0<5)en09_arm_retract(epw,5);else en09_arm_reset(epw);++epw->ct0;
    int n=en09_frames(epw);if(!n)return;
    if((epw->frm_no>>16)==n-1){epw->mtn_add=0;epw->flg|=0x100;
        if(kind<8)e->state&=0xfffe3f7fu;
        else {e->state=(e->state&~15u)|1;e->state&=0xffff7f7fu;epw->flg2&=~1u;}
        en09_damage_end(epw);}
}
/* en09.c:6303: struck while vaulting, horizontal speed from saved ATR. */
static void en09_dg05(RECVX_ENEMY_WORK* epw){
    RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){epw->flg2|=1;e->state=(e->state&~0x1bfu)|2;epw->flg&=~0x100u;
        int f=epw->frm_no>>16;if(epw->mtn_no==9&&f<6)e->state|=0x200;else if(epw->mtn_no==12&&f>5)e->state&=~0x200u;
        en09_motion(epw,23,8);e->state|=0x20000000;en09_arm_deltas(epw);en09_extra_set(e,0x58,0);
        if(!e->route_rec_valid){en09_gap(epw,"EN09 DG05: saved ATR");return;}
        const RECVX_ENEMY_ATR* a=&e->route_rec;if(a->type==0){int angle=epw->axp&65535;epw->axp=angle;float len;int back=(e->state&0x200)!=0;
            if(angle>0x2000&&angle<=0x5fff)len=-(back?12+a->px+a->w-epw->px:a->px-9-epw->px);
            else if(angle>0x6000&&angle<=0x9fff)len=back?a->pz-12-epw->pz:9+a->pz+a->d-epw->pz;
            else if(angle>0xa000&&angle<=0xdfff)len=back?a->px-12-epw->px:9+a->px+a->w-epw->px;
            else len=-(back?12+a->pz+a->d-epw->pz:a->pz-9-epw->pz);
            en09_extra_set(e,0x58,(len/15)/njCos(0x2000));}
        epw->ct0=0;++epw->mode3;return;
    }
    if(epw->mode3==1){int n=en09_frames(epw);if(!n)return;float floor=epw->sv->grand((unsigned)(epw->flr_no+2));
        if((epw->frm_no>>16)==n-1){epw->mtn_add=0;epw->py=.001f+floor;epw->flg|=0x100;epw->flg2&=~1u;e->state=(e->state&0xffff3f70u)|1;en09_damage_end(epw);}
        else {if(epw->ct0<5)en09_arm_retract(epw,5);else en09_arm_reset(epw);
            float v=en09_extra_float(e,0x58);if(epw->ct0<15){epw->spd=v*njCos(0x2000);en09_speed(epw,0);}
            epw->py+=fabsf(v)*njSin(0x2000)-epw->ct0*.23f;if(epw->py<floor)epw->py=.001f+floor;++epw->ct0;}}
}
/* en09.c:6863: finishing motion before corpse. */
static void en09_dg10(RECVX_ENEMY_WORK* epw){RECVX_EN09_EXT* e=en09_ext(epw);
    if(epw->mode3==0){unsigned m=epw->mtn_no;int frame=0;
        if(m==39||m==23){if(m==23)frame=0xf0000;m=48;}else if(m==56||m==53){if(m==53)frame=0x100000;m=55;}else m=0xffffffffu;
        if(m!=0xffffffffu){recvx_t3_chg_mtn(epw,m,frame,0);e->state&=0x1fffffffu;}++epw->mode3;}
    int n=en09_frames(epw);if(!n)return;
    if(epw->mode3==1&&(epw->frm_no>>16)==n-1){epw->mtn_add=0;epw->mode0=4;epw->mode1=epw->mode2=epw->mode3=0;e->state=(e->state&0xffff3e70u)|1;}
    else if(epw->mode3==2&&(epw->frm_no>>16)==n-1)epw->mode3=0;
}
