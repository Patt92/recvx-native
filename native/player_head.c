/* Kopfsteuerung des Spielers (Auftrag B06).
 *
 * Neu geschriebener nativer Code nach den in player_head.h genannten
 * Originalroutinen.  Es wurde kein Funktionskoerper uebernommen und kein
 * Generator benutzt; die Quarantaene wurde weder gelesen noch verwendet.
 *
 * Aufbau des Originalverfahrens, wie es hier umgesetzt ist:
 *
 *   bhControlPlayerHead sichert zuerst Kopfpunkt und Winkelsumme des Vorbildes
 *   (EXP1_F(24..32) <- EXP1_F(12..20), EXP1_S(42..46) <- EXP1_S(36..40)) und
 *   bildet dann die Summe der lokalen Winkel.  Die Summe laeuft in drei
 *   Stufen: Knoten 0..3 vor der Kopfarbeit, danach Knoten 4, danach Knoten 5 -
 *   dazwischen veraendert die Kopfarbeit genau diese beiden Knoten.
 *
 *   Eine laufende Skriptfolge (EXP1_UC(120) & 0xF0) hat Vorrang: sie zaehlt
 *   zwei Werte gegen ihre Grenzen und legt sie auf Knoten 4 und 5.
 *
 *   Sonst sucht bhLookNearEnemy ein Ziel (nur wenn 0x4 steht und 0x8 nicht),
 *   und bhSetHeadRotation bildet daraus die Winkel - aber erst, wenn 0x10
 *   steht und der Zielindex nicht negativ ist.  Ohne Ziel werden die
 *   gehaltenen Winkel von der laufenden Motion uebernommen, jeweils nur solange
 *   das zugehoerige Rueckfuehrbit nicht steht.
 *
 *   Die vier Rueckfuehrungen (0x20/0x40/0x80/0x100) ziehen Knoten 4 und 5 je
 *   Bild um ein Viertel der Differenz zurueck und loeschen ihr Bit, sobald der
 *   Abstand 182 (ein Grad) nicht mehr ueberschritten wird.
 *
 *   Am Ende haelt EXP1_F(12..20) den Kopfpunkt dieses Bildes, und fuer alle
 *   Modelle ausser Nummer 3 laeuft die Blinkfolge ueber PlyEyeTab.
 */
#include "player_head.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <string.h>

/* player.c:206-209, unveraenderte Originaldaten. */
static const signed char PlyEyeTab[8]={0,0,1,1,2,2,1,0};

/* player.c:6884 u.a.: ein Grad in der Winkeleinheit des Originals. */
#define HEAD_RETURN_THRESHOLD 182
/* player.c:7161-7185: Grenzen der Blickwinkel. */
#define HEAD_PITCH_LIMIT 6144
#define HEAD_YAW_LIMIT   8192
/* player.c:7142: ab dieser Entfernung folgt der gehaltene Punkt dem Ziel. */
#define HEAD_POINT_FOLLOW 1.6f
/* player.c:7015: Startwert des Suchfensters von bhSearchNearEnemy2. */
#define HEAD_SEARCH_WINDOW 23665
/* player.c:7021: so lange muss ein Ziel gehalten werden, bevor es gilt. */
#define HEAD_LOOK_SETTLE 30
/* player.c:7046: Obergrenze des Haltezaehlers. */
#define HEAD_LOOK_TIMER_MAX 1024
/* player.c:6974/6981: Schwelle und Obergrenze der Blinkpause. */
#define HEAD_BLINK_CHANCE 0.97f
#define HEAD_BLINK_READY  30
#define HEAD_BLINK_IDLE   60
/* Winkelumrechnung wie im Original (player.c:7158/7174, pwksub.c:494). */
#define HEAD_ANGLE_FROM_ATAN 10430.381f

static int16_t head_abs16(int32_t v){return (int16_t)(v<0?-v:v);}

void recvx_head_begin(RECVX_HEAD_STATE* s,int model_number){
    /* bhSetPlayer (player.c:787-789) nullt exp1 und setzt genau diese zwei
     * Felder: ((int*)exp1)[0] = 1 und ((short*)exp1)[34] = -1.  Offset 34
     * short entspricht Byte 68, also target_index. */
    memset(s,0,sizeof *s);
    s->flags=RECVX_HEAD_BLINK_ENABLE;
    s->target_index=-1;
    s->model_number=model_number;
    s->eye_texture=-1;
}

void recvx_head_release_look(RECVX_HEAD_STATE* s){
    /* Die Folge, die alle abschaltenden Stellen des Originals benutzen:
     * EXP1_I(0) &= ~0x4; EXP1_I(0) |= 0x1E0. */
    s->flags&=~RECVX_HEAD_LOOK;
    s->flags|=RECVX_HEAD_RETURN_ALL;
}

void recvx_head_enable_look(RECVX_HEAD_STATE* s){
    s->flags|=RECVX_HEAD_LOOK;
}

/* bhSearchNearEnemy2, pwksub.c:395-503.  Liefert 1 und setzt `index`,
 * `window` und `distance`, wenn ein Ziel im Winkel- und Sichtfenster liegt. */
static int head_search(RECVX_HEAD_STATE* s,const RECVX_HEAD_INPUT* in,
                       const RECVX_HEAD_CANDIDATE* list,unsigned count,
                       const RECVX_HEAD_HOST* host,
                       int32_t* window,float* distance,int32_t* index){
    NJS_MATRIX m;
    NJS_VECTOR unit={0,0,-1.0f},facing,edge,to;
    float limit,nearest=0,length;
    int32_t best=-1,chosen_window=*window;
    float best_point[3]={0,0,0};
    unsigned i;

    if(chosen_window==0)chosen_window=32768; /* pwksub.c:418-421 */

    /* Blickachse des Spielers und die Kante des Suchfensters. */
    njUnitMatrix(&m);
    njRotateY(&m,in->ay);
    njCalcVector(&m,&unit,&facing);
    njRotateY(&m,chosen_window);
    njCalcVector(&m,&unit,&edge);
    limit=-njInnerProduct(&facing,&edge); /* pwksub.c:435 */

    for(i=0;i<count;++i){
        if(!list[i].eligible)continue;
        /* pwksub.c:460-462: der Vektor laeuft vom Ziel zum Kopfpunkt. */
        to.x=s->head[0]-list[i].point[0];
        to.y=s->head[1]-list[i].point[1];
        to.z=s->head[2]-list[i].point[2];
        length=njScalor(&to);
        if(njInnerProduct(&facing,&to)<limit&&(length<nearest||nearest==0)){
            nearest=length;best=list[i].index;
            best_point[0]=list[i].point[0];
            best_point[1]=list[i].point[1];
            best_point[2]=list[i].point[2];
        }
    }

    if(best<0)return 0;

    /* pwksub.c:483-486: ohne freie Sicht gilt das Ziel nicht.  Die Kapsel
     * laeuft von (plp->px, Kopfhoehe, plp->pz) zum Zielpunkt. */
    {
        const float from[3]={in->position[0],s->head[1],in->position[2]};
        if(!host||!host->wall_clear){
            if(host&&host->unbound)host->unbound(host->user,"bhCheckC2WallN line of sight");
            return 0;
        }
        if(!host->wall_clear(host->user,from,best_point))return 0;
    }

    /* pwksub.c:488-497: die Richtung zum Ziel in der XZ-Ebene. */
    {
        NJS_VECTOR flat={best_point[0]-s->head[0],0,best_point[2]-s->head[2]};
        njUnitVector(&flat);
        *window=(int32_t)(HEAD_ANGLE_FROM_ATAN*atan2f(-flat.x,-flat.z));
    }
    *distance=nearest;
    *index=best;
    return 1;
}

/* bhLookNearEnemy, player.c:7009-7050. */
static void head_look(RECVX_HEAD_STATE* s,const RECVX_HEAD_INPUT* in,
                      const RECVX_HEAD_CANDIDATE* list,unsigned count,
                      const RECVX_HEAD_HOST* host){
    int32_t window=HEAD_SEARCH_WINDOW,index=-1;
    float distance=s->head[1]-in->position[1];

    if(head_search(s,in,list,count,host,&window,&distance,&index)){
        if(s->look_timer>HEAD_LOOK_SETTLE){
            /* Ein anderes Ziel setzt den Haltezaehler zurueck. */
            if(s->target_index!=(int16_t)index)s->look_timer=0;
            s->flags|=RECVX_HEAD_LOCKED;
            s->target_kind=RECVX_HEAD_TARGET_ENEMY;
            s->target_index=(int16_t)index;
            /* ene[id].lok_jno */
            s->target_node=0;
            {unsigned i;for(i=0;i<count;++i)if(list[i].index==index){s->target_node=(int16_t)list[i].look_node;break;}}
        }
    }else if(s->flags&RECVX_HEAD_LOCKED){
        /* Ziel verloren: die vier Rueckfuehrungen laufen an. */
        s->flags|=RECVX_HEAD_RETURN_ALL;
        s->flags&=~RECVX_HEAD_LOCKED;
        s->target_index=-1;
        s->look_timer=0;
    }

    if(s->look_timer<HEAD_LOOK_TIMER_MAX)s->look_timer++;
}

/* bhSetHeadRotation, player.c:7053-7204. */
static void head_rotation(RECVX_HEAD_STATE* s,const RECVX_HEAD_INPUT* in,
                          const RECVX_HEAD_CANDIDATE* list,unsigned count,
                          const RECVX_HEAD_HOST* host,
                          int16_t ax,int16_t ay,int32_t angles[6][3]){
    const int32_t node=s->target_node;
    float look[3]={s->target_point[0],s->target_point[1],s->target_point[2]};
    int resolved=0;
    int16_t pax,pay,lax,lay;

    switch(s->target_kind){
    case RECVX_HEAD_TARGET_POINT:
        /* player.c:7067-7073: der gehaltene Punkt selbst. */
        s->target_index=0;
        resolved=1;
        break;
    case RECVX_HEAD_TARGET_ENEMY:
        /* player.c:7075-7078: die Grenze ist sys->ewk_n, die Zahl der
         * Gegnerwerke - nicht die Laenge der gefilterten Kandidatenliste. */
        if(in->enemy_count<=s->target_index)s->target_index=0;
        {unsigned i;for(i=0;i<count;++i)if(list[i].index==s->target_index){
            if(node==0){look[0]=list[i].point[0];look[1]=list[i].point[1];look[2]=list[i].point[2];resolved=1;}
            else if(host&&host->target_node_point)
                resolved=host->target_node_point(host->user,RECVX_HEAD_TARGET_ENEMY,s->target_index,node,look);
            break;}}
        break;
    case RECVX_HEAD_TARGET_OBJECT:
    case RECVX_HEAD_TARGET_ITEM:
    case RECVX_HEAD_TARGET_EFFECT:
        /* player.c:7082-7105: Objekt-, Item- und Effektarbeiten.  Diese
         * Zielarten setzt im Original nur ein Skript; sie brauchen denselben
         * Knotendienst. */
        if(host&&host->target_node_point)
            resolved=host->target_node_point(host->user,s->target_kind,s->target_index,node,look);
        else if(host&&host->unbound)
            host->unbound(host->user,"bhSetHeadRotation object/item/effect target");
        break;
    case RECVX_HEAD_TARGET_ROOMPOS:
        /* player.c:7106-7119: rom->posp, Knoten wird dabei genullt. */
        s->target_node=0;
        if(host&&host->target_node_point)
            resolved=host->target_node_point(host->user,RECVX_HEAD_TARGET_ROOMPOS,s->target_index,0,look);
        else if(host&&host->unbound)
            host->unbound(host->user,"bhSetHeadRotation room position target");
        break;
    default:
        return;
    }
    if(!resolved)return;

    /* player.c:7138-7153: der gehaltene Punkt folgt dem Ziel erst ab 1.6
     * Abstand, und nur solange der Blick nicht angehalten ist. */
    {
        const float dx=look[0]-s->target_point[0];
        const float dy=look[1]-s->target_point[1];
        const float dz=look[2]-s->target_point[2];
        if(njSqrt(dx*dx+dy*dy+dz*dz)>HEAD_POINT_FOLLOW&&!(s->flags&RECVX_HEAD_LOOK_HOLD)){
            s->target_point[0]=look[0];s->target_point[1]=look[1];s->target_point[2]=look[2];
        }else{
            look[0]=s->target_point[0];look[1]=s->target_point[1];look[2]=s->target_point[2];
        }
    }

    /* player.c:7155-7169: Neigung gegen die Kopfhoehe, Gegenrichtung zur
     * bereits aufgebrachten Summe, dann begrenzt. */
    {
        const float px=look[0]-in->position[0];
        const float pz=look[2]-in->position[2];
        int32_t value=-(int32_t)(HEAD_ANGLE_FROM_ATAN*atan2f(look[1]-s->head[1],njSqrt(px*px+pz*pz)));
        value=-(value+ax);
        if(value> HEAD_PITCH_LIMIT)value= HEAD_PITCH_LIMIT;
        if(value<-HEAD_PITCH_LIMIT)value=-HEAD_PITCH_LIMIT;
        pax=(int16_t)(value/2); /* player.c:7187 */
    }
    /* player.c:7171-7188: Gier, um eine halbe Drehung versetzt. */
    {
        const float px=look[0]-in->position[0];
        const float pz=look[2]-in->position[2];
        int32_t value=-(int32_t)(HEAD_ANGLE_FROM_ATAN*atan2f(px,pz))+32768;
        value=-(value+ay);
        if(value> HEAD_YAW_LIMIT)value= HEAD_YAW_LIMIT;
        if(value<-HEAD_YAW_LIMIT)value=-HEAD_YAW_LIMIT;
        pay=(int16_t)(value/2);
    }

    /* player.c:7190-7203: ein Viertel der Differenz je Bild, und beide Knoten
     * erhalten denselben Winkel. */
    lax=(int16_t)(s->node4[0]+(int16_t)((int16_t)(pax-s->node4[0])/4));
    lay=(int16_t)(s->node4[1]+(int16_t)((int16_t)(pay-s->node4[1])/4));
    s->node4[0]=lax;s->node4[1]=lay;
    s->node5[0]=lax;s->node5[1]=lay;
    angles[4][0]=s->node4[0];angles[4][1]=s->node4[1];
    angles[5][0]=s->node5[0];angles[5][1]=s->node5[1];
}

/* Eine Rueckfuehrung: ein Viertel der Differenz, Bit loeschen bei <= 182.
 * player.c:6879-6911 fuer Knoten 4 und :6925-6957 fuer Knoten 5.  Das
 * Original bildet die Differenz fuer Knoten 4 als (pose - gehalten) und fuer
 * Knoten 5 als (pose - gehalten) mit getauschten Rollen der Variablen; das
 * Ergebnis ist in beiden Faellen derselbe Schritt zur Pose hin. */
static void head_return(RECVX_HEAD_STATE* s,uint32_t bit,int16_t* held,int32_t* pose){
    const int16_t target=(int16_t)*pose;
    if(head_abs16(target-*held)>HEAD_RETURN_THRESHOLD){
        *held=(int16_t)(*held+(int16_t)((int16_t)(target-*held)/4));
        *pose=*held;
    }else{
        s->flags&=~bit;
    }
}

void recvx_head_tick(RECVX_HEAD_STATE* s,const RECVX_HEAD_INPUT* in,
                     const RECVX_HEAD_CANDIDATE* list,unsigned count,
                     const RECVX_HEAD_HOST* host,int32_t angles[6][3]){
    int16_t ax,ay,az;
    unsigned i,j;

    for(i=0;i<6;++i)for(j=0;j<3;++j)angles[i][j]=in->angles[i][j];
    s->eye_texture=-1;

    /* player.c:6753-6762: Kopfpunkt und Winkelsumme des Vorbildes sichern. */
    s->head_prev[0]=s->head[0];s->head_prev[1]=s->head[1];s->head_prev[2]=s->head[2];
    s->sum_prev[0]=s->sum[0];s->sum_prev[1]=s->sum[1];s->sum_prev[2]=s->sum[2];

    /* player.c:6760-6774: Summe der Knoten 0..3 plus plp->ax/ay/az.  Das
     * Original fuehrt ax/ay/az als short, die Kuerzung ist uebernommen. */
    {
        int32_t sx=in->ax,sy=in->ay,sz=in->az;
        for(i=0;i<4;++i){sx+=in->angles[i][0];sy+=in->angles[i][1];sz+=in->angles[i][2];}
        ax=(int16_t)sx;ay=(int16_t)sy;az=(int16_t)sz;
    }

    if(s->script_flags&RECVX_HEAD_SCRIPT_ACTIVE){
        /* player.c:6776-6818: die Skriptfolge hat Vorrang.  Y-Stufe zuerst,
         * danach X; beide laufen gegen ihre eigene Grenze. */
        if(s->script_flags&RECVX_HEAD_SCRIPT_YAW_DOWN){
            s->script_value[1]-=s->script_step[1];
            if(s->script_limit[1]>=s->script_value[1])s->script_value[1]=s->script_limit[1];
        }else{
            s->script_value[1]+=s->script_step[1];
            if(s->script_value[1]>=s->script_limit[1])s->script_value[1]=s->script_limit[1];
        }
        angles[4][1]=s->script_value[1];
        if(s->script_flags&RECVX_HEAD_SCRIPT_PITCH_DOWN){
            s->script_value[0]-=s->script_step[0];
            if(s->script_limit[0]>=s->script_value[0])s->script_value[0]=s->script_limit[0];
        }else{
            s->script_value[0]+=s->script_step[0];
            if(s->script_value[0]>=s->script_limit[0])s->script_value[0]=s->script_limit[0];
        }
        angles[4][0]=s->script_value[0];
    }else{
        int rotated=0;
        if(s->flags&RECVX_HEAD_LOOK){
            if(!(s->flags&RECVX_HEAD_LOOK_HOLD))head_look(s,in,list,count,host);
            if((s->flags&RECVX_HEAD_LOCKED)&&s->target_index>=0){
                head_rotation(s,in,list,count,host,ax,ay,angles);
                rotated=1;
            }
        }
        if(!rotated){
            /* player.c:6835-6876: ohne Ziel folgen die gehaltenen Winkel der
             * Motion, jeweils nur solange ihr Rueckfuehrbit nicht steht. */
            if(!(s->flags&RECVX_HEAD_RETURN_N4_X))s->node4[0]=(int16_t)angles[4][0];
            if(!(s->flags&RECVX_HEAD_RETURN_N4_Y))s->node4[1]=(int16_t)angles[4][1];
            if(!(s->flags&RECVX_HEAD_RETURN_N5_X))s->node5[0]=(int16_t)angles[5][0];
            if(!(s->flags&RECVX_HEAD_RETURN_N5_Y))s->node5[1]=(int16_t)angles[5][1];
        }
        /* player.c:6879-6911: die beiden Rueckfuehrungen des Knotens 4. */
        if(s->flags&RECVX_HEAD_RETURN_N4_X)head_return(s,RECVX_HEAD_RETURN_N4_X,&s->node4[0],&angles[4][0]);
        if(s->flags&RECVX_HEAD_RETURN_N4_Y)head_return(s,RECVX_HEAD_RETURN_N4_Y,&s->node4[1],&angles[4][1]);
    }

    /* player.c:6914-6916: Knoten 4 geht in die Summe ein. */
    ax=(int16_t)(ax+angles[4][0]);
    ay=(int16_t)(ay+angles[4][1]);
    az=(int16_t)(az+angles[4][2]);

    if(s->script_flags&RECVX_HEAD_SCRIPT_ACTIVE){
        /* player.c:6918-6922: dieselben Werte auch auf Knoten 5. */
        angles[5][1]=s->script_value[1];
        angles[5][0]=s->script_value[0];
    }else{
        if(s->flags&RECVX_HEAD_RETURN_N5_X)head_return(s,RECVX_HEAD_RETURN_N5_X,&s->node5[0],&angles[5][0]);
        if(s->flags&RECVX_HEAD_RETURN_N5_Y)head_return(s,RECVX_HEAD_RETURN_N5_Y,&s->node5[1],&angles[5][1]);
    }

    /* player.c:6960-6966: Knoten 5 schliesst die Summe ab. */
    ax=(int16_t)(ax+angles[5][0]);
    ay=(int16_t)(ay+angles[5][1]);
    az=(int16_t)(az+angles[5][2]);
    s->sum[0]=ax;s->sum[1]=ay;s->sum[2]=az;

    /* player.c:6968-6970: der Kopfpunkt dieses Bildes aus owP[5].mtx. */
    s->head[0]=in->head[12];s->head[1]=in->head[13];s->head[2]=in->head[14];

    /* player.c:6972-7005: Blinkfolge.  Modell 3 hat keine Augentextur. */
    if(s->model_number!=3){
        if(in->random>HEAD_BLINK_CHANCE&&s->mode0!=4&&!(s->blink_timer<=HEAD_BLINK_READY)){
            s->flags|=RECVX_HEAD_BLINK_RUN;
            s->blink_frame=0;
            s->blink_timer=0;
        }else if(s->blink_timer<HEAD_BLINK_IDLE){
            s->blink_timer++;
        }
        if(s->flags&RECVX_HEAD_BLINK_ENABLE){
            s->eye_texture=PlyEyeTab[s->blink_frame&7];
            if(s->flags&RECVX_HEAD_BLINK_RUN){
                s->blink_frame++;
                if(s->blink_frame>7){
                    s->flags&=~RECVX_HEAD_BLINK_RUN;
                    s->blink_frame=0;
                    s->blink_timer=0;
                    s->eye_texture=0;
                }
            }
        }
    }
}
