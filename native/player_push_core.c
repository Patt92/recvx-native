/* Schiebbare Kisten und Schiebeaktion des Spielers.
 *
 * Neu geschriebener nativer Code nach den Originalroutinen
 *   bhObj001                       objitm.c:835-1176  (bhJumpObject[1])
 *   bhControlObjItm                objitm.c:288-       (Durchlaufbedingungen)
 *   bhCheckWallType2               hitchk.c:2046-2175
 *   bhCheckWall2Box                hitchk.c:1276-1936
 *   bhCheckInnerTriangle2          hitchk.c:3896-4083
 *   bhCheckInnerP4                 hitchk.c (unmittelbar danach)
 *   bhCheckBox2Box                 hitchk.c:4327-
 *   njCollisionCheckBS/SC          ps2_NaColi.c:445-640
 *   bhControlPlayerPad             player.c:1820-1954
 *   bhCPM2_act_psh                 player.c:3728-3884
 *   bhCPM2_act_wlk (Einstieg)      player.c:2403-2410
 *   bhSetMotion (Bildvorschub)     Motion.c:79-112, flg 0x400000 player.c:1504
 *   bhCheckFloorNum/bhAddSpeed     pwksub.c:112-141
 * Es wurde kein Funktionskoerper uebernommen.  PlMtnAct (player.c:216) ist
 * eine unveraenderte Originaltabelle; die Masse der sieben Kistenvarianten,
 * die Schiebegeraeusche und alle Schwellen stehen so im Original.
 *
 * Besitz: Die Kisten-O_WRKs und ihre sys->mwalp-/metcp-/mflrp-Eintraege
 * gehoeren diesem Kern; es gibt keinen zweiten.  Die Spielerfelder kommen als
 * Sicht der einen Spielerarbeit herein (RECVX_PUSH_STATE) und gehen wieder
 * hinaus.  Raumwaende (rom->walp) und vorher vergebene Gegnereintraege
 * reicht der Host als Praefix herein; der Kern veraendert sie nicht. */
#include "player_push_core.h"
#include "combat_geometry.h"
#include "ninja_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* player.c:216-227, unveraenderte Originaldaten. */
static const unsigned short PlMtnAct[2][3][7]={
    {{42,39,0,4,16,52,9},{43,40,2,7,18,52,9},{44,41,3,8,19,52,9}},
    {{42,39,1,6,17,54,9},{43,40,2,7,18,54,9},{44,41,3,8,19,54,9}}};

/* Die O_WRK-Felder, die bhObj001, bhCheckWallType2 und bhCheckWall2Box an
 * einer Kiste lesen und schreiben.  bhCheckWall2Box bekommt die Kiste als
 * BH_PWORK; die Felder liegen in beiden Strukturen an derselben Stelle
 * (types.h), daher auch stflg und psh_ct. */
typedef struct {
    unsigned flg,stflg,mdflg;
    int flr_no,mode0,aspd,param,idx_ct,psh_ct;
    float px,py,pz,aw,ah,ad;
    int pn,ct0,ani_ct,ct1,ct2,ct3;
} PUSH_OBJ;

struct RECVX_PUSH_CORE {
    RECVX_PUSH_HOST host;
    float room_h;                          /* rom->h */
    float grand[32];                       /* rom->grand */
    unsigned sp_flg;
    PUSH_OBJ* obj;unsigned obj_n;
    RECVX_STAIR_AREA* prefix;unsigned prefix_n,prefix_cap;
    RECVX_STAIR_AREA* mwal;unsigned mwal_n; /* nur die Kisteneintraege */
    RECVX_STAIR_AREA* metc;unsigned metc_n;
    RECVX_STAIR_AREA* mflr;unsigned mflr_n;
    unsigned* frames;unsigned frame_n;
    float* trans;unsigned trans_n;
    int psh_snd;                           /* sys->psh_snd */
};

/* ------------------------------------------------------------- Hilfsdienste */

static int push_floor_num(const RECVX_PUSH_CORE* c,float py)
{
    int fno=2,i;
    for(i=0;i<31;++i)
        if((c->grand[i]!=0.0f&&py>=c->grand[i])||(i==2&&py>=c->grand[i]))fno=i;
    return fno-2;
}

static float push_grand(const RECVX_PUSH_CORE* c,int floor)
{
    int i=floor+2;
    if(i<0)i=0;
    if(i>31)i=31;
    return c->grand[i];
}

static float push_height(const RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA* a)
{
    return a->h!=0.0f?a->h:c->room_h;
}

static unsigned push_wall_n(const RECVX_PUSH_CORE* c){return c->prefix_n+c->mwal_n;}

static RECVX_STAIR_AREA* push_wall(RECVX_PUSH_CORE* c,unsigned i)
{
    return i<c->prefix_n?&c->prefix[i]:&c->mwal[i-c->prefix_n];
}

static float push_p2p(RECVX_CB_VEC a,RECVX_CB_VEC b)
{
    float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
    return njSqrt(dx*dx+dy*dy+dz*dz);
}

/* njCollisionCheckSC der Ninja-Bibliothek: naechster Punkt der Geraden,
 * an das naehere Kapselende geklemmt, gegen die Summe der Radien. */
static int push_sphere_capsule(RECVX_CB_VEC centre,float sr,RECVX_CB_VEC c1,RECVX_CB_VEC c2,float cr)
{
    RECVX_CB_LINE line;
    RECVX_CB_VEC near;
    float length;
    line.px=c1.x;line.py=c1.y;line.pz=c1.z;
    line.vx=c2.x-c1.x;line.vy=c2.y-c1.y;line.vz=c2.z-c1.z;
    if(cr+sr<=recvx_cb_distance_p2l(&centre,&line,&near))return 0;
    length=push_p2p(c1,c2);
    if(length<push_p2p(near,c1))near=c2;
    else if(length<push_p2p(near,c2))near=c1;
    return push_p2p(centre,near)<cr+sr;
}

/* njCollisionCheckBS mit v[0] als Ecke kleinster x/groesster y/z und v[6]
 * als Gegenecke; danach die zwoelf Kanten als Kapseln mit Radius 0. */
static int push_box_sphere(const RECVX_CB_VEC v[8],RECVX_CB_VEC centre,float r)
{
    static const unsigned char edge[12][2]={
        {0,3},{0,4},{0,1},{5,1},{5,4},{5,6},{2,6},{2,1},{2,3},{7,3},{7,4},{7,6}};
    unsigned i;
    if(centre.x<=v[0].x-r||v[6].x+r<=centre.x||centre.y<=v[6].y-r||v[0].y+r<=centre.y||
       centre.z<=v[6].z-r||v[0].z+r<=centre.z)return 0;
    if(v[0].x<=centre.x&&centre.x<=v[6].x&&v[6].z<=centre.z&&centre.z<=v[0].z)return 1;
    if(v[6].y<=centre.y&&centre.y<=v[0].y&&v[6].z<=centre.z&&centre.z<=v[0].z)return 1;
    if(v[0].x<=centre.x&&centre.x<=v[6].x&&v[6].y<=centre.y&&centre.y<=v[0].y)return 1;
    for(i=0;i<12;++i)
        if(push_sphere_capsule(centre,r,v[edge[i][0]],v[edge[i][1]],0.0f))return 1;
    return 0;
}

/* bhCheckInnerP4: Streifen nach z geordnet, untere Grenzen einschliesslich,
 * obere ausschliesslich.  Die Steigungen werden wie im Original vorab je
 * Abschnitt bestimmt, nicht ueber eine allgemeine Kantenformel. */
static int push_inner_p4(float x,float z,const float p[4][2])
{
    float nn,x0=0.0f,x1=0.0f,y0,y1,y2,y3,lo,hi,t;
    if(z<p[0][1]||z>=p[3][1])return 0;
    nn=p[1][0]-p[0][0];
    y0=p[1][1]-p[0][1];y1=p[2][1]-p[0][1];y2=p[3][1]-p[1][1];y3=p[3][1]-p[2][1];
    if(nn>0.0f){
        if(y1!=0.0f)x0=(p[2][0]-p[0][0])/y1;
        if(y0!=0.0f)x1=nn/y0;
    }else{
        if(y0!=0.0f)x0=nn/y0;
        if(y1!=0.0f)x1=(p[2][0]-p[0][0])/y1;
    }
    if(z<p[1][1]){
        lo=p[0][0]+x0*(z-p[0][1]);
        hi=p[0][0]+x1*(z-p[0][1]);
    }else if(z<p[2][1]){
        if(nn>0.0f){
            x1=y2!=0.0f?(p[3][0]-p[1][0])/y2:0.0f;
            lo=p[0][0]+x0*(z-p[0][1]);
            hi=p[1][0]+x1*(z-p[1][1]);
        }else{
            x0=y2!=0.0f?(p[3][0]-p[1][0])/y2:0.0f;
            lo=p[1][0]+x0*(z-p[1][1]);
            hi=p[0][0]+x1*(z-p[0][1]);
        }
    }else{
        x0=y2!=0.0f?(p[3][0]-p[1][0])/y2:0.0f;
        x1=y3!=0.0f?(p[3][0]-p[2][0])/y3:0.0f;
        if(nn>0.0f){
            lo=p[2][0]+x1*(z-p[2][1]);
            hi=p[1][0]+x0*(z-p[1][1]);
        }else{
            lo=p[1][0]+x0*(z-p[1][1]);
            hi=p[2][0]+x1*(z-p[2][1]);
        }
    }
    if(lo>hi){t=lo;lo=hi;hi=t;}
    return !(lo>x||hi<=x);
}

/* bhCheckInnerTriangle2: Dreieckswand gegen ein achsparalleles Rechteck mit
 * den Halbmassen aw/ad.  Der Bandtest laeuft ueber die vier Punkte der
 * Hypotenuse, versetzt um aw/ad, danach der Diagonaltest des Originals. */
static int push_inner_triangle(const RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA* a,float x,float y,float z,float aw,float ad,float ah)
{
    float h=push_height(c,a),w=a->w,d=a->d,lo_x,hi_x,lo_z,hi_z,px,pz,band[4][2];
    if(a->py>y+ah||a->py+h<y)return 0;
    if(w>0.0f){lo_x=a->px;hi_x=aw+(lo_x+w);}else{hi_x=a->px;lo_x=(hi_x+w)-aw;}
    if(d>0.0f){lo_z=a->pz;hi_z=ad+(lo_z+d);}else{hi_z=a->pz;lo_z=(hi_z+d)-ad;}
    if(lo_x>x||hi_x<=x||lo_z>z||hi_z<=z)return 0;
    if(aw==0.0f&&ad==0.0f)
        return !(fabsf((a->pz+a->d)-z)<=fabsf(d/(w/(x-a->px))));
    px=a->px;pz=a->pz;
    switch(a->id){
    case 0:
        band[0][0]=px+a->w;band[0][1]=pz;
        band[1][0]=band[0][0]+aw;band[1][1]=band[0][1]+ad;
        band[2][0]=px;band[2][1]=pz+a->d;
        band[3][0]=band[2][0]+aw;band[3][1]=band[2][1]+ad;
        if(push_inner_p4(x,z,band))return 1;
        px+=aw;pz+=ad;
        if(px>x||pz>z)return 0;
        break;
    case 1:
        band[0][0]=px+aw;band[0][1]=(pz+a->d)-ad;
        band[1][0]=band[0][0]-aw;band[1][1]=band[0][1]+ad;
        band[2][0]=aw+(px+a->w);band[2][1]=pz-ad;
        band[3][0]=band[2][0]-aw;band[3][1]=band[2][1]+ad;
        if(push_inner_p4(x,z,band))return 1;
        px+=aw;pz-=ad;
        if(px>x||pz<=z)return 0;
        break;
    case 2:
        band[0][0]=px+a->w;band[0][1]=pz;
        band[1][0]=band[0][0]-aw;band[1][1]=band[0][1]+ad;
        band[2][0]=px;band[2][1]=pz+a->d;
        band[3][0]=band[2][0]-aw;band[3][1]=band[2][1]+ad;
        if(push_inner_p4(x,z,band))return 1;
        px-=aw;pz+=ad;
        if(px<=x||pz>z)return 0;
        break;
    case 3:
        band[0][0]=px-aw;band[0][1]=(pz+a->d)-ad;
        band[1][0]=band[0][0]+aw;band[1][1]=band[0][1]+ad;
        band[2][0]=(px+a->w)-aw;band[2][1]=pz-ad;
        band[3][0]=band[2][0]+aw;band[3][1]=band[2][1]+ad;
        if(push_inner_p4(x,z,band))return 1;
        px-=aw;pz-=ad;
        if(px<=x||pz<=z)return 0;
        break;
    default:
        /* id > 3 hat im Original keinen Zweig; px/pz bleiben die Ecke. */
        break;
    }
    return !(fabsf((pz+d)-z)<=fabsf(d/(w/(x-px))));
}

/* bhCheckBox2Box: Rechteckaufloesung an einer (auch null-breiten) Box;
 * die Seite waehlt die Lage gegen die Boxmitte, die Achse der kleinere
 * Abstand zur aufgeweiteten Kante. */
static int push_box_box(const RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA* a,float* x,float y,float* z,float aw,float ad,float ah)
{
    float h=push_height(c,a),cx=a->px+0.5f*a->w,cz=a->pz+0.5f*a->d;
    float px=a->px-aw,pz=a->pz-ad,xn=a->w+2.0f*aw,zn=a->d+2.0f*ad,ex,ez;
    if(!(*x-px>=0.0f&&*x-px<xn&&*z-pz>=0.0f&&*z-pz<zn&&y+ah>=a->py&&y<=a->py+h))return 0;
    ex=*x<cx?px:px+xn;
    ez=*z<cz?pz:pz+zn;
    if(fabsf(*x-ex)>fabsf(*z-ez))*z=ez;else *x=ex;
    return 1;
}

/* Die Quadrantenaufloesung von bhCheckWall2Box (Typ 0/1 und Typ 7): Seite
 * nach der Lage gegen die Wandmitte, Achse nach dem groesseren Abstand. */
static void push_out(PUSH_OBJ* o,float px,float pz,float xn,float zn,float cx,float cz)
{
    float ex=o->px<cx?px:px+xn,ez=o->pz<cz?pz:pz+zn;
    if(fabsf(o->px-ex)>fabsf(o->pz-ez))o->pz=ez;else o->px=ex;
}

/* ----------------------------------------------------------- Wandabfragen */

/* bhCheckWallType2: erster Eintrag, der das Rechteck pos +- aw/ad mit der
 * Hoehe ah beruehrt.  Kistenwaende mit attr 0x10000 der eigenen Kiste
 * (prm3 == idx_ct) und attr-0x40-Eintraege fallen bei flg 0x2000 heraus. */
static const RECVX_STAIR_AREA* push_wall_type2(RECVX_PUSH_CORE* c,float x,float y,float z,unsigned flg,float aw,float ad,float ah,int idx_ct)
{
    unsigned i,n=push_wall_n(c);
    for(i=0;i<n;++i){
        const RECVX_STAIR_AREA* a=push_wall(c,i);
        float h,px,pz,xn,zn;
        if(!(a->flg&1u)||((a->attr&0x40u)&&(flg&0x2000u)))continue;
        h=push_height(c,a);
        switch(a->type){
        case 0:case 1:
            if((a->type&1u)&&(flg&0x400u))break;
            if((a->attr&0x10000u)&&(flg&0x2000u)&&a->prm3==(unsigned char)idx_ct)break;
            pz=a->pz-ad;xn=a->w+2.0f*aw;zn=a->d+2.0f*ad;px=x-(a->px-aw);
            if(px>=0.0f&&px<xn&&z-pz>=0.0f&&z-pz<zn&&y+ah>=a->py&&y<=a->py+h)return a;
            break;
        case 2:case 3:
            if((a->type&1u)&&(flg&0x400u))break;
            px=(a->px-aw)-a->w;pz=(a->pz-ad)-a->w;
            xn=2.0f*a->w+2.0f*aw;zn=2.0f*a->w+2.0f*ad;
            if(x-px>=0.0f&&x-px<xn&&z-pz>=0.0f&&z-pz<zn&&y+ah>=a->py&&y<=a->py+h){
                RECVX_CB_VEC v[8],centre;
                unsigned k;
                for(k=0;k<4;++k){v[k].y=0.1f;v[k+4].y=0.0f;}
                v[0].x=v[4].x=x-aw;v[0].z=v[4].z=z+ad;
                v[1].x=v[5].x=x-aw;v[1].z=v[5].z=z-ad;
                v[2].x=v[6].x=x+aw;v[2].z=v[6].z=z-ad;
                v[3].x=v[7].x=x+aw;v[3].z=v[7].z=z+ad;
                centre.x=a->px;centre.y=a->py;centre.z=a->pz;
                if(push_box_sphere(v,centre,a->w))return a;
            }
            break;
        case 4:case 5:
            if((a->type&1u)&&(flg&0x400u))break;
            if(push_inner_triangle(c,a,x,y,z,aw,ad,ah))return a;
            break;
        case 6:
            if(x-a->px>=0.0f&&x-a->px<a->w&&z-a->pz>=0.0f&&z-a->pz<a->d&&y+ah>=a->py&&y<=a->py+h)return a;
            break;
        case 7:
            /* Typ 7 liest hier hp->h unmittelbar, ohne rom->h-Ersatz. */
            pz=a->pz-ad;xn=a->w+2.0f*aw;zn=a->d+2.0f*ad;px=a->px-aw;
            if(x-px>=0.0f&&x-px<xn&&z-pz>=0.0f&&z-pz<zn){
                float top=a->py+a->h;
                if((flg&0x100u)?(y<top&&y+ah>top):(y+ah>=top&&y<a->py))return a;
            }
            break;
        default:
            break;
        }
    }
    return NULL;
}

/* Gemeinsame Attributfilter der Typen 0/1, 2/3 und 4/5 in bhCheckWall2Box. */
static int push_box_filter(const RECVX_STAIR_AREA* a,const PUSH_OBJ* o)
{
    return !((a->type&1u)&&(o->flg&0x400u))&&!((a->attr&1u)&&a->flr_no!=o->flr_no)&&
           !((a->attr&2u)&&(o->flg&0x4000u))&&!((a->attr&4u)&&(o->stflg&0x40000000u));
}

/* bhCheckWall2Box fuer eine Kiste (pw == op, nie plp). */
static void push_box_walls(RECVX_PUSH_CORE* c,PUSH_OBJ* o)
{
    unsigned i,n=push_wall_n(c);
    int hit=0;
    for(i=0;i<n;++i){
        RECVX_STAIR_AREA* a=push_wall(c,i);
        float h,px,pz,xn,zn,cx,cz;
        if(!(a->flg&1u))continue;
        h=push_height(c,a);
        switch(a->type){
        case 0:case 1:
            if(!push_box_filter(a,o))break;
            if((a->attr&0x10000u)&&(o->flg&0x2000u)&&a->prm3==(unsigned char)o->idx_ct)break;
            cx=a->px+0.5f*a->w;cz=a->pz+0.5f*a->d;
            px=a->px-o->aw;pz=a->pz-o->ad;xn=a->w+2.0f*o->aw;zn=a->d+2.0f*o->ad;
            if(!(o->px-px>=0.0f&&o->px-px<xn&&o->pz-pz>=0.0f&&o->pz-pz<zn&&o->py+o->ah>=a->py&&o->py<=a->py+h))break;
            ++hit;
            /* attr 0x10000 zaehlt psh_ct nur fuer plp; eine Kiste ist es nie. */
            if(!(a->attr&0x20000u)&&(o->stflg&0x80u))a->attr|=0x20000u; /* psh_idx der Kiste: kein Leser */
            push_out(o,px,pz,xn,zn,cx,cz);
            break;
        case 2:case 3:{
            float abx,abz,ln,sine,cosine;
            int r;
            if(!push_box_filter(a,o)||((a->attr&0x40u)&&(o->flg&0x2000u)))break;
            px=(a->px-o->aw)-a->w;pz=(a->pz-o->ad)-a->w;
            xn=2.0f*a->w+2.0f*o->aw;zn=2.0f*a->w+2.0f*o->ad;
            if(!(o->px-px>=0.0f&&o->px-px<xn&&o->pz-pz>=0.0f&&o->pz-pz<zn&&o->py+o->ah>=a->py&&o->py<=a->py+h))break;
            /* Flaechenseiten: die Achse mit dem kleineren Abstand zur
             * aufgeweiteten Kante, nur wenn die andere Achse den Kreis traegt. */
            {
                const float right=o->aw+(a->px+a->w),back=o->ad+(a->pz+a->w);
                const int east=a->px<o->px,south=a->pz<o->pz;
                abx=fabsf(o->px-(east?right:px));
                abz=fabsf(o->pz-(south?back:pz));
                if(abx<abz){
                    if(fabsf(a->pz-o->pz)<o->ad){++hit;o->px=east?right:px;}
                }else if(fabsf(a->px-o->px)<o->aw){++hit;o->pz=south?back:pz;}
            }
            /* Danach die vier Ecken der Kiste gegen den Kreis, in der Reihenfolge
             * (-aw,-ad), (+aw,-ad), (-aw,+ad), (+aw,+ad); nur die erste wirkt. */
            {
                static const float corner[4][2]={{-1.0f,-1.0f},{1.0f,-1.0f},{-1.0f,1.0f},{1.0f,1.0f}};
                unsigned k;
                for(k=0;k<4;++k){
                    float ox=corner[k][0]<0.0f?o->px-o->aw:o->px+o->aw;
                    float oz=corner[k][1]<0.0f?o->pz-o->ad:o->pz+o->ad;
                    px=a->px-ox;pz=a->pz-oz;
                    ln=njSqrt(px*px+pz*pz);
                    if(!(ln<a->w))continue;
                    ++hit;
                    r=(int)(10430.381f*atan2f(px,pz));
                    njSinCos(r,&sine,&cosine);
                    o->px=corner[k][0]<0.0f?o->aw+(a->px-sine*a->w):(a->px-sine*a->w)-o->aw;
                    o->pz=corner[k][1]<0.0f?o->ad+(a->pz-cosine*a->w):(a->pz-cosine*a->w)-o->ad;
                    break;
                }
            }
            break;}
        case 4:case 5:
            if(!push_box_filter(a,o))break;
            if(push_inner_triangle(c,a,o->px,o->py,o->pz,o->aw,o->ad,o->ah)){
                RECVX_CB_LINE line;
                RECVX_CB_VEC at,near;
                ++hit;
                px=a->px;pz=a->pz;
                switch(a->id){
                case 0:px=a->px+o->aw;pz=a->pz+o->ad;break;
                case 1:px=a->px+o->aw;pz=a->pz-o->ad;break;
                case 2:px=a->px-o->aw;pz=a->pz+o->ad;break;
                case 3:px=a->px-o->aw;pz=a->pz-o->ad;break;
                default:break;
                }
                line.px=px;line.py=0.0f;line.pz=pz+a->d;
                line.vx=a->w;line.vy=0.0f;line.vz=-a->d;
                at.x=o->px;at.y=o->py;at.z=o->pz;
                recvx_cb_distance_p2l(&at,&line,&near);
                o->px=near.x;o->pz=near.z;
            }else{
                /* Die beiden Katheten als null-breite Boxen, jeweils von der
                 * rechten Winkelecke aus; die Lage wird dabei mitgezogen. */
                RECVX_STAIR_AREA leg=*a;
                leg.py=a->py;leg.h=a->h;
                switch(a->id){
                case 0:
                    leg.px=a->px;leg.pz=a->pz;leg.w=0.0f;leg.d=a->d;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    leg.w=a->w;leg.d=0.0f;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    break;
                case 1:
                    leg.px=a->px;leg.pz=a->pz+a->d;leg.w=0.0f;leg.d=-a->d;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    leg.pz=a->pz;leg.w=a->w;leg.d=0.0f;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    break;
                case 2:
                    leg.px=a->px;leg.pz=a->pz;leg.w=0.0f;leg.d=a->d;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    leg.px=a->px+a->w;leg.w=-a->w;leg.d=0.0f;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    break;
                case 3:
                    leg.px=a->px;leg.pz=a->pz+a->d;leg.w=0.0f;leg.d=-a->d;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    leg.px=a->px+a->w;leg.pz=a->pz;leg.w=-a->w;leg.d=0.0f;
                    if(push_box_box(c,&leg,&o->px,o->py,&o->pz,o->aw,o->ad,o->ah))++hit;
                    break;
                default:
                    break;
                }
            }
            break;
        case 6:{
            RECVX_CB_LINE line;
            RECVX_CB_VEC at,near;
            int j;
            if(!(o->px-a->px>=0.0f&&o->px-a->px<a->w&&o->pz-a->pz>=0.0f&&o->pz-a->pz<a->d&&
                 o->py+o->ah>=a->py&&o->py<=a->py+h&&(o->flg&0x100u)))break;
            ++hit;
            memset(&line,0,sizeof line);
            switch(a->id){
            case 0:line.px=a->px;line.py=a->py;line.pz=o->pz;line.vx=a->w;line.vy=a->h;line.vz=0.0f;break;
            case 1:line.px=o->px;line.py=a->py;line.pz=a->pz+a->d;line.vx=0.0f;line.vy=a->h;line.vz=-a->d;break;
            case 2:line.px=o->px;line.py=a->py;line.pz=a->pz;line.vx=0.0f;line.vy=a->h;line.vz=a->d;break;
            case 3:line.px=a->px+a->w;line.py=a->py;line.pz=o->pz;line.vx=-a->w;line.vy=a->h;line.vz=0.0f;break;
            default:break;
            }
            at.x=o->px;at.y=o->py;at.z=o->pz;
            recvx_cb_distance_p2l(&at,&line,&near);
            o->py=near.y;
            for(j=0;j<31;++j)
                if((c->grand[j]!=0.0f&&o->py>=c->grand[j])||(j==2&&o->py>=c->grand[j]))o->flr_no=j-2;
            if(o->flr_no!=a->flr_no){
                if(o->py<push_grand(c,o->flr_no)){o->px=near.x;o->pz=near.z;}
                else o->py=push_grand(c,o->flr_no);
            }else{
                o->px=near.x;o->pz=near.z;
                if(o->py<push_grand(c,o->flr_no))o->py=push_grand(c,o->flr_no);
            }
            break;}
        case 7:{
            float top=a->py+a->h;
            if((a->attr&1u)&&a->flr_no!=o->flr_no)break;
            cx=a->px+0.5f*a->w;cz=a->pz+0.5f*a->d;
            px=a->px-o->aw;pz=a->pz-o->ad;xn=a->w+2.0f*o->aw;zn=a->d+2.0f*o->ad;
            if(!(o->px-px>=0.0f&&o->px-px<xn&&o->pz-pz>=0.0f&&o->pz-pz<zn))break;
            if(!((o->flg&0x100u)?(o->py<top&&o->py+o->ah>top):(o->py+o->ah>=top&&o->py<a->py)))break;
            ++hit;
            push_out(o,px,pz,xn,zn,cx,cz);
            break;}
        default:
            break;
        }
    }
    if(hit)o->stflg|=1u;
    if(!hit&&(o->flg&0x100u))o->py=push_grand(c,o->flr_no);
    if(!hit)o->psh_ct=0;
}

/* ------------------------------------------------------------------ bhObj001 */

static RECVX_STAIR_AREA push_area(unsigned char flg,unsigned char type,int floor,unsigned attr,
                                  float px,float py,float pz,float w,float h,float d,
                                  unsigned char prm0,unsigned char prm3)
{
    RECVX_STAIR_AREA a;
    memset(&a,0,sizeof a);
    a.flg=flg;a.type=type;a.flr_no=(signed char)floor;a.attr=attr;
    a.px=px;a.py=py;a.pz=pz;a.w=w;a.h=h;a.d=d;a.prm0=prm0;a.prm3=prm3;
    return a;
}

/* case 0: Masse je Variante (aspd) und die beweglichen Eintraege.  Nur die
 * besteigbaren Varianten 0/6 (pn != 0) bekommen Deckelebene, Stufenflaechen
 * unten/oben und die Bodenschallflaeche. */
static void push_obj_init(RECVX_PUSH_CORE* c,PUSH_OBJ* o,unsigned onow)
{
    int top;
    o->flg|=0x102000u;
    o->mdflg|=0x8u;
    switch(o->aspd){
    case 0:case 6:o->aw=4.5f;o->ah=7.99f;o->ad=4.5f;o->pn=1;break;
    case 1:o->aw=2.5f;o->ah=19.9f;o->ad=6.0f;o->pn=0;break;
    case 2:o->aw=3.5f;o->ah=29.9f;o->ad=3.5f;o->pn=0;break;
    case 3:o->aw=3.0f;o->ah=18.0f;o->ad=7.5f;o->pn=0;break;
    case 4:o->aw=5.0f;o->ah=10.0f;o->ad=2.5f;o->pn=0;break;
    case 5:o->aw=2.5f;o->ah=10.0f;o->ad=5.0f;o->pn=0;break;
    case 7:o->aw=2.25f;o->ah=8.5f;o->ad=5.5f;o->pn=0;break;
    default:break; /* andere aspd lassen aw/ah/ad/pn wie bhSetObject: 0 */
    }
    o->ct0=(int)c->mwal_n;
    c->mwal[c->mwal_n++]=push_area(0x81,0,o->flr_no,0x10002u,o->px-o->aw,push_grand(c,o->flr_no),o->pz-o->ad,
                                   2.0f*o->aw,o->ah,2.0f*o->ad,0,(unsigned char)onow);
    if(o->pn==0)return;
    top=push_floor_num(c,9.0f+o->py);
    o->ani_ct=(int)c->mwal_n;
    c->mwal[c->mwal_n++]=push_area(0x81,7,top,1u,o->px-o->aw,push_grand(c,top),o->pz-o->ad,
                                   2.0f*o->aw,-1.0f,2.0f*o->ad,0,(unsigned char)onow);
    o->ct1=(int)c->metc_n;
    c->metc[c->metc_n++]=push_area(0x81,2,o->flr_no,0u,o->px-o->aw,push_grand(c,o->flr_no),o->pz-o->ad,
                                   2.0f*o->aw,0.0f,2.0f*o->ad,0,0);
    o->ct2=(int)c->metc_n;
    c->metc[c->metc_n++]=push_area(0x81,2,top,0u,o->px-o->aw,push_grand(c,top),o->pz-o->ad,
                                   2.0f*o->aw,0.0f,2.0f*o->ad,1,0);
    o->ct3=(int)c->mflr_n;
    c->mflr[c->mflr_n++]=push_area(0x81,1,top,0u,(o->px-o->aw)-4.0f,push_grand(c,top),(o->pz-o->ad)-4.0f,
                                   8.0f+2.0f*o->aw,0.0f,8.0f+2.0f*o->ad,(unsigned char)o->param,0);
}

/* case 1: Nachbarschaftsbits, Mitnahme durch die Schiebeaktion und das
 * Nachfuehren aller beweglichen Eintraege. */
static void push_obj_run(RECVX_PUSH_CORE* c,PUSH_OBJ* o,RECVX_PUSH_STATE* plp,int psh_idx,unsigned stflg)
{
    RECVX_STAIR_AREA* box=&c->mwal[o->ct0];
    box->attr&=~0x3C0000u;
    /* Vor/hinter/rechts/links der Kiste je ein 0.1 dicker Streifen, 0.2
     * ausserhalb der Kante: belegt -> diese Seite ist gesperrt. */
    if(push_wall_type2(c,o->px,o->py,(o->pz-o->ad)-0.2f,o->flg,o->aw-0.1f,0.1f,o->ah,o->idx_ct))box->attr|=0x40000u;
    if(push_wall_type2(c,o->px,o->py,0.2f+(o->pz+o->ad),o->flg,o->aw-0.1f,0.1f,o->ah,o->idx_ct))box->attr|=0x100000u;
    if(push_wall_type2(c,0.2f+(o->px+o->aw),o->py,o->pz,o->flg,0.1f,o->ad-0.1f,o->ah,o->idx_ct))box->attr|=0x80000u;
    if(push_wall_type2(c,(o->px-o->aw)-0.2f,o->py,o->pz,o->flg,0.1f,o->ad-0.1f,o->ah,o->idx_ct))box->attr|=0x200000u;
    /* push_wall_type2 kann ueber die Kistentabelle laufen, aber nicht in sie
     * schreiben: `box` bleibt gueltig. */
    if(box->attr&0x20000u){
        if(psh_idx!=o->ct0+(int)c->prefix_n){
            plp->phase=6;
            box->attr&=~0x20000u;
        }else{
            switch(o->aspd){
            case 0:c->psh_snd=527;break;
            case 6:c->psh_snd=528;break;
            case 1:case 3:c->psh_snd=529;break;
            case 2:c->psh_snd=531;break;
            case 4:case 5:c->psh_snd=532;break;
            default:c->psh_snd=527;break;
            }
            if((stflg&0x80u)&&plp->phase==5){
                int blocked=0;
                o->px-=plp->speed*njSin(plp->ay);
                o->pz-=plp->speed*njCos(plp->ay);
                push_box_walls(c,o);
                box=&c->mwal[o->ct0];
                switch(plp->ay&0xC000){
                case 0x0000:blocked=(box->attr&0x40000u)!=0;break;
                case 0x4000:blocked=(box->attr&0x200000u)!=0;break;
                case 0x8000:blocked=(box->attr&0x100000u)!=0;break;
                default:blocked=(box->attr&0x80000u)!=0;break;
                }
                if(blocked)plp->phase=6;
            }else box->attr&=~0x20000u;
        }
    }
    box->px=o->px-o->aw;
    box->pz=o->pz-o->ad;
    if(o->pn==0)return;
    c->mwal[o->ani_ct].px=o->px-o->aw;c->mwal[o->ani_ct].pz=o->pz-o->ad;
    c->metc[o->ct1].px=o->px-o->aw;c->metc[o->ct1].pz=o->pz-o->ad;
    c->metc[o->ct2].px=o->px-o->aw;c->metc[o->ct2].pz=o->pz-o->ad;
    c->mflr[o->ct3].px=(o->px-o->aw)-4.0f;c->mflr[o->ct3].pz=(o->pz-o->ad)-4.0f;
}

void recvx_push_control(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* plp,unsigned cb_flg,int psh_idx,unsigned stflg)
{
    unsigned i;
    if(!c||!plp||!(c->sp_flg&0x4u))return;
    /* cb_flg 0x40000000 beschraenkt den Durchlauf auf obwp[0..3], die
     * Kisten liegen ab Index 4. */
    if(cb_flg&0x40000000u)return;
    for(i=0;i<c->obj_n;++i){
        PUSH_OBJ* o=&c->obj[i];
        if((o->stflg&0x1000000u)||!(o->flg&1u))continue;
        if(o->mode0==0){
            if(o->flg&0x100000u){o->mode0=1;continue;}
            push_obj_init(c,o,(unsigned)o->idx_ct);
            o->mode0=1;
        }
        push_obj_run(c,o,plp,psh_idx,stflg);
    }
}

/* ------------------------------------------------------------ Spieleraktion */

static unsigned push_frames(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s,int motion)
{
    if(motion<0||(unsigned)motion>=c->frame_n||!c->frames[motion]){
        s->unsupported_services|=RECVX_PUSH_UNBOUND_MOTION;
        return 0;
    }
    return c->frames[motion];
}

static void push_sound(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s)
{
    if(c->host.sound)c->host.sound(c->host.user,c->psh_snd);
    else s->unsupported_services|=RECVX_PUSH_UNBOUND_SOUND;
    if(c->host.vibration)c->host.vibration(c->host.user,0,8);
    else s->unsupported_services|=RECVX_PUSH_UNBOUND_VIBRATION;
}

/* Gemeinsamer Motionwechsel der Schiebeaktion: Hokan 42598 ueber vier Bilder. */
static void push_motion(RECVX_PUSH_STATE* s,int motion,int add)
{
    s->motion=motion;
    s->hokan_rate=42598;
    s->hokan_count=4;
    s->mtn_add=add;
    s->unsupported_services|=RECVX_PUSH_UNBOUND_HOKAN;
}

int recvx_push_begin(RECVX_PUSH_STATE* s,int psh_ct,float x,float y,float z,int ay,int dmlvl,int wpntp)
{
    if(!s||psh_ct<=5||(s->status&0x80u))return 0;
    s->st_flg|=0x4u;
    s->status|=0x10080u;
    s->active=1;
    s->mode0=1;
    s->mode2=18;
    s->phase=0;
    s->contact=psh_ct;
    s->x=x;s->y=y;s->z=z;s->ay=ay;
    s->dmlvl=dmlvl<0?0:dmlvl>2?2:dmlvl;
    s->wpntp=wpntp?1:0;
    /* bhCPM1_act_bas, digitale Steuerung (gm_flg 0x40 aus): rtspd nach dmlvl. */
    s->rtspd=s->dmlvl==2?0.8f:1.0f;
    return 1;
}

static void push_turn(RECVX_PUSH_STATE* s,int sign)
{
    int ayn=(short)s->ay;
    int diff=(int)(short)s->ayp-ayn;
    if((diff<0?-diff:diff)<1310){s->phase=3;return;}
    s->ay+=sign*(int)(182.04445f*(7.2f*s->rtspd));
    s->mtn_add=sign*65536;
}

void recvx_push_tick(RECVX_PUSH_CORE* c,RECVX_PUSH_STATE* s,int forward)
{
    unsigned frm_num;
    int total,sample,finished=0;
    if(!c||!s||!s->active)return;
    /* bhCPM0_action */
    s->mtn_md=0;
    /* bhControlPlayerPad: nur das Vorwaertsbit haelt die Schiebeaktion. */
    if(!forward&&(s->status&0x80u)){
        s->flags|=0x200000u;
        if(s->phase==5)s->phase=6;
    }else{
        s->flags&=~0x200000u;
        s->status&=~0x400u;
        s->st_flg&=~0x4u;
        s->flags|=0x200000u;
    }
    /* bhCPM2_act_psh */
    s->st_flg|=0x4u;
    switch(s->phase){
    case 0:{
        int ayn;
        s->status|=0x10080u;
        s->flags&=~0x28C0000u;
        s->flg2|=0x1u;
        push_motion(s,PlMtnAct[s->wpntp][s->dmlvl][1],65536);
        s->arp=0.0f;
        s->frm_no=0;
        s->ayp=(s->ay+8192)&~0x3FFF;
        ayn=(short)s->ay;
        s->phase=((short)((short)s->ayp-ayn)>=0)?2:1;
        break;}
    case 1:push_turn(s,-1);break;
    case 2:push_turn(s,1);break;
    case 3:
        push_motion(s,49,65536);
        s->frm_no=0;
        s->ay=(s->ay+8192)&~0x3FFF;
        s->phase=4;
        break;
    case 4:
        s->flags|=0x200000u;
        if(s->flags&0x400000u){
            push_motion(s,50,65536);
            s->frm_no=0;
            s->phase=5;
            push_sound(c,s);
        }
        break;
    case 5:{
        int key=s->frm_no/65536+1;
        s->flags|=0x10000000u;
        if(s->flags&0x400000u)push_sound(c,s);
        if(!c->trans||key<0||(unsigned)key>=c->trans_n){
            s->unsupported_services|=RECVX_PUSH_UNBOUND_TRANS;
            s->speed=0.0f;
        }else s->speed=c->trans[key];
        s->x-=s->speed*njSin(s->ay&0xFFFF);
        s->z-=s->speed*njCos(s->ay&0xFFFF);
        break;}
    case 6:
        s->flags&=~0x10000000u;
        push_motion(s,49,-32768);
        s->mtn_md=8;
        frm_num=push_frames(c,s,49);
        s->frm_no=frm_num?(int)(frm_num-1u)*65536:0;
        s->phase=7;
        break;
    case 7:
        s->mtn_md=8;
        if(s->frm_no/65536<=0){
            s->mode0=1;
            s->mtn_add=0;
            s->status&=~0x10080u;
            finished=1;
        }
        break;
    default:
        break;
    }
    /* bhSetMotion: abtasten am aktuellen frm_no, dann vorschieben. */
    sample=s->frm_no;
    frm_num=push_frames(c,s,s->motion);
    if(frm_num){
        total=(int)(frm_num<<16);
        if((s->mtn_md&0x1)&&s->frm_mode!=0)s->frm_no-=s->mtn_add;
        else s->frm_no+=s->mtn_add;
        if(s->mtn_md&0x1){
            if(s->frm_no<=0||s->frm_no>=total-65536)s->frm_mode=~s->frm_mode;
        }else if(s->frm_no<0)s->frm_no+=total;
        else if(s->frm_no>=total)s->frm_no-=total;
        if(s->frm_no/65536>=(int)frm_num-1)s->flags|=0x400000u;
        else s->flags&=~0x400000u;
    }
    s->frame=sample;
    s->ay&=0xFFFF;
    /* Ende von bhCPM1_act_bas: mode2 != 3 -> psh_ct = 0 (der Host setzt seinen
     * Zaehler zurueck).  Mit dem Abschlussbild gibt die Aktion die Steuerung
     * an bhControlPlayerPad zurueck. */
    if(finished)s->active=0;
}

/* ------------------------------------------------------------ Verwaltung */

RECVX_PUSH_CORE* recvx_push_create(float room_height,const float* grand,const RECVX_PUSH_OBJECT* objects,unsigned count,const RECVX_PUSH_HOST* host)
{
    RECVX_PUSH_CORE* c=calloc(1,sizeof *c);
    unsigned i;
    if(!c)return NULL;
    if(host)c->host=*host;
    c->room_h=room_height;
    if(grand)memcpy(c->grand,grand,sizeof c->grand);
    c->sp_flg=5;
    c->obj_n=count;
    c->obj=calloc(count?count:1,sizeof *c->obj);
    c->mwal=calloc(count?2*count:1,sizeof *c->mwal);
    c->metc=calloc(count?2*count:1,sizeof *c->metc);
    c->mflr=calloc(count?count:1,sizeof *c->mflr);
    if(!c->obj||!c->mwal||!c->metc||!c->mflr){recvx_push_destroy(c);return NULL;}
    for(i=0;i<count;++i){
        PUSH_OBJ* o=&c->obj[i];
        /* bhSetObject: npSetMemory(0), dann die ETTY_WORK-Felder. */
        o->flg=objects[i].flags;
        if(o->flg&0x80000000u)o->mdflg|=0x10u;
        if(o->flg&0x20000000u)o->mdflg|=0x40u;
        o->flr_no=objects[i].floor;
        o->param=objects[i].param;
        o->aspd=objects[i].aspd;
        o->idx_ct=objects[i].index;
        o->px=objects[i].x;o->py=objects[i].y;o->pz=objects[i].z;
    }
    return c;
}

void recvx_push_destroy(RECVX_PUSH_CORE* c)
{
    if(!c)return;
    free(c->obj);free(c->prefix);free(c->mwal);free(c->metc);free(c->mflr);free(c->frames);free(c->trans);
    free(c);
}

void recvx_push_motions(RECVX_PUSH_CORE* c,const unsigned* frames,unsigned count,const float* trans_z,unsigned trans_count)
{
    if(!c)return;
    free(c->frames);free(c->trans);c->frames=NULL;c->trans=NULL;c->frame_n=c->trans_n=0;
    if(count&&frames&&(c->frames=malloc(count*sizeof *c->frames))){memcpy(c->frames,frames,count*sizeof *c->frames);c->frame_n=count;}
    if(trans_count&&trans_z&&(c->trans=malloc(trans_count*sizeof *c->trans))){memcpy(c->trans,trans_z,trans_count*sizeof *c->trans);c->trans_n=trans_count;}
}

void recvx_push_enabled(RECVX_PUSH_CORE* c,unsigned sp_flg){if(c)c->sp_flg=sp_flg;}

void recvx_push_update_walls(RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA* prefix,unsigned n)
{
    if(!c)return;
    if(n>c->prefix_cap){
        RECVX_STAIR_AREA* grown=realloc(c->prefix,n*sizeof *grown);
        if(!grown)return;
        c->prefix=grown;c->prefix_cap=n;
    }
    if(n)memcpy(c->prefix,prefix,n*sizeof *prefix);
    c->prefix_n=n;
}

void recvx_push_update_object(RECVX_PUSH_CORE* c,unsigned i,float x,float y,float z)
{
    if(!c||i>=c->obj_n)return;
    c->obj[i].px=x;c->obj[i].py=y;c->obj[i].pz=z;
}

void recvx_push_object(RECVX_PUSH_CORE* c,unsigned i,float out[3])
{
    out[0]=out[1]=out[2]=0.0f;
    if(!c||i>=c->obj_n)return;
    out[0]=c->obj[i].px;out[1]=c->obj[i].py;out[2]=c->obj[i].pz;
}

unsigned recvx_push_wall_count(RECVX_PUSH_CORE* c){return c?c->mwal_n:0;}

void recvx_push_wall_attributes(RECVX_PUSH_CORE* c,const unsigned* attr,unsigned n)
{
    unsigned i;
    if(!c||!attr)return;
    for(i=0;i<n&&i<c->mwal_n;++i)c->mwal[i].attr=attr[i];
}

unsigned recvx_push_walls(RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA** out){*out=c?c->mwal:NULL;return c?c->mwal_n:0;}
unsigned recvx_push_steps(RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA** out){*out=c?c->metc:NULL;return c?c->metc_n:0;}
void recvx_push_step_attr(RECVX_PUSH_CORE* c,unsigned index,unsigned set_bits,unsigned clear_bits){
    if(!c||index>=c->metc_n)return;
    c->metc[index].attr=(c->metc[index].attr|set_bits)&~clear_bits;
}
unsigned recvx_push_floors(RECVX_PUSH_CORE* c,const RECVX_STAIR_AREA** out){*out=c?c->mflr:NULL;return c?c->mflr_n:0;}
