/* Treppen-, Leiter- und Stufenaktionen des Spielers (Auftrag B01).
 *
 * Neu geschriebener nativer Code nach den Originalroutinen
 *   bhCPM2_act_kdu/kdd/dnu/dnd/hsu/hsd   player.c:2872-4321
 *   bhSetMotion                          Motion.c:21-120
 *   bhGetObjMotion                       Motion.c:919-975
 *   bhCheckFloorNum/bhSetFloorNum/bhAddSpeed  pwksub.c:86-141
 *   bhCheckFloorSound                    hitchk.c:5434-5467
 *   bhCheckWater                         hitchk.c:5532-5558
 *   bhSetEffect/bhSetEffectTb            effect.c:331-...
 * Es wurde kein Funktionskoerper uebernommen; die Prozeduren sind hier als
 * eigenstaendiger Code formuliert.  Die Tabellen PlMtnAct/PlFootSnd/PlKDU sind
 * Originaldaten (player.c:216-281).
 *
 * Der Zustand ist eine Sicht auf die eine vorhandene Spielerarbeit des Hosts:
 * `flg`, `status` (stflg), `flg2`, `mode0`, `ax`/`ay`/`az`, `motion`, `frame`,
 * `floor`, `rtspd`, `arp` und `mtn_*` werden von der Laufzeit vor
 * dem Aufruf hinein- und danach zurueckgeschrieben.  Die Buchfuehrung des
 * Originalwerks (spx/spz, bpx/bpy/bpz, pxb/pyb/pzb und ihre b-Vorgaenger)
 * gehoert der vorhandenen Bewegungs-/Kollisionsarbeit und wird hier nicht
 * doppelt gefuehrt.
 */
#include "player_stairs.h"
#include "ninja_matrix.h"
#include <string.h>

/* player.c:216-227, unveraenderte Originaldaten. */
static const unsigned short PlMtnAct[2][3][7]={
    {{42,39,0,4,16,52,9},{43,40,2,7,18,52,9},{44,41,3,8,19,52,9}},
    {{42,39,1,6,17,54,9},{43,40,2,7,18,54,9},{44,41,3,8,19,54,9}}};
/* player.c:239-257.  Alle vier Reihen sind im Original gleich. */
static const signed char PlFootSnd[4][2][7]={
    {{10,8,10,10,0,21,20},{28,18,28,30,16,8,8}},
    {{10,8,10,10,0,21,20},{28,18,28,30,16,8,8}},
    {{10,8,10,10,0,21,20},{28,18,28,30,16,8,8}},
    {{10,8,10,10,0,21,20},{28,18,28,30,16,8,8}}};
/* player.c:258-281. */
static const signed char PlKDU[4][2][3]={
    {{19,6,15},{19,6,15}},{{21,10,15},{24,18,15}},
    {{21,10,15},{21,14,15}},{{24,18,15},{24,18,15}}};

/* player.c:214, unveraenderte Originaldaten: die Knotenkette vom Wurzelknoten
 * bis zum rechten bzw. linken Fuss, mit der bhFixPosition die Figur an ihrem
 * aufgesetzten Fuss festhaelt. */
static const signed char PlyLegRoute[2][7]={
    {0,1,14,15,16,17,-1},
    {0,1,18,19,20,21,-1}};

/* ABS() ist im Original ein ganzzahliges Betrags-Makro. */
static int stair_abs(int value){return value<0?-value:value;}

/* bhSetEffect uebernimmt POINT.ox/oy/oz unveraendert in lox/loy/loz; die
 * Aufrufer reichen dort (POINT*)&plp->px herein, sodass die int-Winkel ax/ay/az
 * als float-Bitmuster ankommen.  Das wird hier nicht geglaettet. */
static void stair_bits(int value,float* out)
{
    unsigned bits=(unsigned)value;
    memcpy(out,&bits,sizeof(bits));
}

/* plp->frm_no / 65536; nach dem Umlauf ist der Zaehler nicht negativ. */
static int stair_frame(const RECVX_STAIR_STATE* s){return s->frame/65536;}

/* bhCPM2_act_* case 1 und case 2: die Giernaeherung an plp->ayp ist in allen
 * sechs Routinen wortgleich.  Rueckgabe 1, wenn der Fall behandelt wurde. */
static int stair_approach(RECVX_STAIR_STATE* s)
{
    int ayn;
    if(s->mode3!=1&&s->mode3!=2)return 0;
    ayn=(short)s->ay;
    if(stair_abs((int)(short)s->ayp-ayn)<1310)
    {
        s->mode3=3;
    }
    else if(s->mode3==1)
    {
        s->ay-=(int)(182.04445f*(7.2f*s->rtspd));
        s->mtn_add=-65536;
    }
    else
    {
        s->ay+=(int)(182.04445f*(7.2f*s->rtspd));
        s->mtn_add=65536;
    }
    return 1;
}

/* bhCPM2_act_* case 0: in allen sechs Routinen gleich bis auf die Hokanrate
 * und die ayp-Formel. */
static void stair_open(RECVX_STAIR_STATE* s,const RECVX_STAIR_HOST* host,int hokan_rate,int ayp)
{
    int ayn;
    s->flg&=~0x28C0008u;
    /* EXP1_I(0) &= ~0x4; EXP1_I(0) |= 0x1E0: die Kopfsteuerung gibt ihr Blickziel
     * auf und laesst die vier Rueckfuehrungen laufen. */
    if(host&&host->release_head_look)host->release_head_look(host->user);
    else s->unsupported_services|=RECVX_STAIR_UNBOUND_HEADWORK;
    s->flg2|=0x1u;
    s->motion=PlMtnAct[s->wpntp][s->dmlvl][1];
    s->hokan_rate=hokan_rate;
    s->hokan_count=4;
    s->mtn_add=65536;
    s->arp=0.0f;
    s->frame=0;
    s->ayp=ayp;
    ayn=(short)s->ay;
    s->mode3=((short)((short)s->ayp-ayn)>=0)?2:1;
}

/* bhGetObjMotion(plp, 0, &pos, &ang): Objekt 0 der laufenden Motion.  Ohne
 * Schluessel werden Lage und Winkel genullt (Motion.c:919-975). */
static void stair_obj_motion(const RECVX_STAIR_STATE* s,const RECVX_STAIR_HOST* h,NJS_POINT3* pos)
{
    pos->x=0.0f;pos->y=0.0f;pos->z=0.0f;
    if(!h->motion_root)return;
    if(!h->motion_root(h->user,s->motion,s->frame,&pos->x))
    {
        pos->x=0.0f;pos->y=0.0f;pos->z=0.0f;
        return;
    }
    /* f_flp: nur die X-Komponente des Schluessels wird bei rueckwaerts
     * laufender Motion gespiegelt; mtn_md ist bei den Stufenaktionen nie 2. */
    if(s->mtn_md&0x2)pos->x=-pos->x;
}

/* bhCheckFloorNum ueber die Original-Gelaendetabelle (rom->grand). */
static int stair_floor_num(const RECVX_STAIR_HOST* h,float py)
{
    if(!h->floor_num)return 0;
    return h->floor_num(h->user,py);
}

/* bhSetFloorNum: setzt flr_no und merkt sich den vorherigen Wert in flr_nob. */
static void stair_set_floor(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    int previous=s->floor;
    s->floor=stair_floor_num(h,s->py);
    if(s->floor!=previous)s->floor_b=previous;
}

/* bhAddSpeed(plp, 0). */
static void stair_add_speed(RECVX_STAIR_STATE* s)
{
    int angle=s->ay&0xFFFF;
    s->px-=s->spd*njSin(angle);
    s->pz-=s->spd*njCos(angle);
}

/* bhCheckFloorSound: die Laufzeit bekommt die Original-Operanden, nicht eine
 * vorberechnete Etage.  `special` ist genau plp->stflg & 0x10. */
static int stair_floor_sound(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s,int floor,float x,float z)
{
    if(!h->floor_sound)return 0;
    return h->floor_sound(h->user,floor,x,z,(s->status&0x10u)?1:0);
}

/* plp->mlwP->owP[node].mtx[12..14]. */
static void stair_node(const RECVX_STAIR_HOST* h,unsigned node,NJS_POINT3* out)
{
    out->x=0.0f;out->y=0.0f;out->z=0.0f;
    if(h->node_position)h->node_position(h->user,node,&out->x);
}

static void stair_footstep(const RECVX_STAIR_HOST* h,int sound,int type,int flag)
{
    if(h->footstep)h->footstep(h->user,sound,type,flag);
}

static void stair_effect(const RECVX_STAIR_HOST* h,const RECVX_STAIR_EFFECT* effect)
{
    if(h->effect)h->effect(h->user,effect);
}

/* Der gemeinsame Abschluss (kdu case 6, kdd case 5, hsu/hsd case 6): Figur auf
 * die neue Etage setzen, Arbeit zurueckgeben, Ruhemotion aufsetzen.  `py` ist
 * die Etagenhoehe, `y_offset` der Originalzusatz (0, 0, +/-4*prm2).  kdd laeuft
 * ueber denselben Block, ruft aber bhClrUseKaidanFlag bereits vorher. */
static void stair_finish(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s,float py,unsigned clear_status,float y_offset)
{
    NJS_POINT3 node;
    if(h->kaidan_flag)h->kaidan_flag(h->user,0);
    stair_node(h,0,&node);
    s->px=node.x;
    s->pz=node.z;
    s->py=py+y_offset;
    s->status&=~clear_status;
    s->flg&=~0xD0400u;
    s->flg|=0x118u;
    stair_set_floor(h,s);
    if(h->grand)s->py=h->grand[s->floor+2];
    s->mode0=1;
    s->motion=PlMtnAct[s->wpntp][s->dmlvl][0];
    s->hokan_rate=49152;
    s->hokan_count=8;
    s->mtn_add=65536;
    s->frame=0;
    s->mtn_md|=0x100;
    s->active=0;
}

/* Wurzelbewegung von Objekt 0 auf die Figur uebertragen.  Das Original ruft
 * njUnitMatrix/njRotateY/njCalcPoint mit NULL und schreibt damit in die global
 * haengende Matrix; hier steht dieselbe Rechnung mit lokaler Matrix. */
static void stair_root_carry(RECVX_STAIR_STATE* s,const NJS_POINT3* pos,float* out_x,float* out_y,float* out_z)
{
    NJS_MATRIX matrix;
    NJS_POINT3 world;
    njUnitMatrix(&matrix);
    njRotateY(&matrix,s->ay);
    njCalcPoint(&matrix,(NJS_POINT3*)pos,&world);
    *out_x=world.x;
    *out_y=world.y;
    *out_z=world.z;
}

/* bhCalcTree (MdlPut.c:267): owP[n].mtx ist das Produkt von T(px,py,pz),
 * R(ax,ay,az) und je Knoten der Kette T(objP.pos) R(objP.ang).  Die Kette
 * `route` ist im Original der Weg von der Wurzel zum Fuss, also genau dieser
 * Weg.  `motion`/`frame` waehlen die Pose, die SetMtn* in objP geschrieben
 * hat.  Rueckgabe 0, wenn ein Kettenknoten im nativen Modell fehlt. */
static int stair_chain_point(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s,const signed char* route,
                             unsigned motion,int frame,float px,float py,float pz,int ax,int ay,int az,
                             NJS_POINT3* out)
{
    NJS_MATRIX matrix;
    NJS_POINT3 zero;
    const signed char* node;
    zero.x=zero.y=zero.z=0.0f;
    njUnitMatrix(&matrix);
    njTranslate(&matrix,px,py,pz);
    njRotateXYZ(&matrix,ax,ay,az);
    for(node=route;*node!=-1;++node)
    {
        float pos[3];
        int ang[3];
        if(!h->local_node||!h->local_node(h->user,motion,frame,(unsigned)*node,pos,ang))
        {
            s->unsupported_services|=RECVX_STAIR_UNBOUND_FIXPOS;
            return 0;
        }
        njTranslate(&matrix,pos[0],pos[1],pos[2]);
        njRotateXYZ(&matrix,ang[0],ang[1],ang[2]);
    }
    njCalcPoint(&matrix,&zero,out);
    return 1;
}

/* bhFixPosition (Motion.c:857) und bhFixPositionXYZ (player.c:1216) ueber
 * bhCalcFixOffset mit offP == NULL: der Versatz ist die Verschiebung des
 * Kettenendes zwischen der jetzt abgetasteten Pose und der Pose, mit der
 * owP zuletzt berechnet wurde.  Er wird von der Figurenlage abgezogen, der
 * Fuss bleibt also stehen und die Wurzelbewegung der Motion wandert in
 * plp->px/py/pz. */
static void stair_fix_position(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s,int sample,int xyz)
{
    const signed char* route=PlyLegRoute[(s->flg&0x80000u)?1:0];
    NJS_POINT3 dst,src;
    /* Vor dem ersten eigenen bhCalcModel gibt es kein owP dieser Aktion.  Der
     * Fall tritt nicht auf, weil case 0 aller sechs Routinen plp->flg
     * 0x28C0008 und damit auch 0x40000 loescht (player.c:2880/3143/3357/
     * 3516/3942/4137); der Riegel steht nur, damit hier nie eine fremde Pose
     * als Vorgaenger einfliesst. */
    if(!s->draw_valid)return;
    if(!stair_chain_point(h,s,route,s->motion,sample,s->px,s->py,s->pz,s->ax,s->ay,s->az,&dst))return;
    if(!stair_chain_point(h,s,route,s->draw_motion,s->draw_frame,s->draw_px,s->draw_py,s->draw_pz,
                          s->draw_ax,s->draw_ay,s->draw_az,&src))return;
    s->px-=dst.x-src.x;
    if(xyz)s->py-=dst.y-src.y;
    s->pz-=dst.z-src.z;
}

/* ---------------------------------------------------------------- kdu (14) */
static void kdu(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 pos,pos2,node;
    int dlvl,frame;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,42598,-((int)(182.04445f*(s->area.prm1*90))&0xFFFF));
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=(s->area.prm2%4)==0?35:31;
        if(s->dmlvl>=2)s->motion+=2;
        s->ct2=s->area.prm2;
        s->ct3=s->area.prm2/4;
        s->kdn_fr=stair_floor_num(h,s->py+(2.0f*s->area.prm2));
        if(h->grand)s->yn=h->grand[s->kdn_fr+2];
        s->hokan_rate=42598;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=-((int)(182.04445f*(s->area.prm1*90))&0xFFFF);
        s->spd=1.0f;
        stair_add_speed(s);
        stair_obj_motion(s,h,&pos);
        s->kdn_py=pos.y;
        s->mode3=4;
        /* faellt durch */
    case 4:
        s->mode3=5;
        /* faellt durch */
    case 5:
        stair_obj_motion(s,h,&pos);
        s->flg|=0x200000u;
        if(s->flg&0x400000u)
        {
            if(s->ct3>0)--s->ct3;
            stair_root_carry(s,&pos,&pos2.x,&pos2.y,&pos2.z);
            s->px+=pos2.x;
            s->py+=pos2.y-s->kdn_py;
            s->pz+=pos2.z;
            s->frame=0;
        }
        dlvl=s->dmlvl>=2?1:0;
        frame=stair_frame(s);
        if(((s->ct2%4)==0&&s->ct3==1&&frame>=PlKDU[s->ply_id][dlvl][0])
           ||((s->ct2%4)!=0&&s->ct3==0&&frame>=PlKDU[s->ply_id][dlvl][1]))
        {
            s->flg|=0x8040000u;
            s->flg&=~0x80000u;
            s->motion=PlMtnAct[s->wpntp][s->dmlvl][2];
            s->hokan_rate=42598;
            s->hokan_count=4;
            s->mtn_add=65536;
            s->mtn_md|=0x100;
            s->frame=PlKDU[s->ply_id][dlvl][2]*65536;
            s->ct1=s->dmlvl<2?10:4;
            s->status&=~0x10u;
            stair_node(h,21,&node);
            stair_footstep(h,stair_floor_sound(h,s,s->kdn_fr,node.x,node.z),0,1);
            s->status|=0x210u;
            s->mode3=6;
            break;
        }
        if(frame==PlFootSnd[s->ply_id][0][5])
        {
            stair_node(h,17,&node);
            stair_footstep(h,stair_floor_sound(h,s,stair_floor_num(h,node.y),node.x,node.z),0,1);
            s->status|=0x200u;
        }
        if(frame==PlFootSnd[s->ply_id][1][5])
        {
            stair_node(h,21,&node);
            stair_footstep(h,stair_floor_sound(h,s,stair_floor_num(h,node.y),node.x,node.z),0,1);
            s->status|=0x200u;
        }
        break;
    case 6:
        --s->ct1;
        if(s->ct1<=0)stair_finish(h,s,s->yn,0x80010010u,0.0f);
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- kdd (15) */
static void kdd(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 pos,pos2,node;
    float py;
    int frame;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,42598,-((int)(182.04445f*(s->area.prm1*90))&0xFFFF));
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=(s->area.prm2%4)==0?36:32;
        if(s->dmlvl>=2)s->motion+=2;
        s->hokan_rate=42598;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=-((int)(182.04445f*(s->area.prm1*90))&0xFFFF);
        s->spd=2.0f;
        stair_add_speed(s);
        stair_obj_motion(s,h,&pos);
        s->kdn_py=pos.y;
        s->mode3=4;
        /* faellt durch */
    case 4:
        s->floor=stair_floor_num(h,s->area.py-(2.0f*s->area.prm2));
        s->mode3=5;
        /* faellt durch */
    case 5:
        stair_obj_motion(s,h,&pos);
        py=s->py+pos.y;
        s->flg|=0x200000u;
        if(s->flg&0x400000u)
        {
            stair_root_carry(s,&pos,&pos2.x,&pos2.y,&pos2.z);
            s->px+=pos2.x;
            s->py+=pos2.y-s->kdn_py;
            s->pz+=pos2.z;
            s->frame=0;
        }
        if(py<=(s->area.py-(2.0f*s->area.prm2)))
        {
            s->status&=~0x80010010u;
            stair_node(h,17,&node);
            stair_footstep(h,stair_floor_sound(h,s,s->floor,node.x,node.z),0,1);
            s->status|=0x200u;
            if(h->kaidan_flag)h->kaidan_flag(h->user,0);
            stair_node(h,0,&node);
            s->px=node.x;
            s->pz=node.z;
            s->py=s->area.py-(2.0f*s->area.prm2);
            s->spd=2.0f;
            stair_add_speed(s);
            s->flg&=~0xD0400u;
            s->flg|=0x118u;
            stair_set_floor(h,s);
            if(h->grand)s->py=h->grand[s->floor+2];
            s->mode0=1;
            s->motion=PlMtnAct[s->wpntp][s->dmlvl][0];
            s->hokan_rate=49152;
            s->hokan_count=8;
            s->mtn_add=65536;
            s->frame=0;
            s->mtn_md|=0x100;
            s->active=0;
            break;
        }
        frame=stair_frame(s);
        if(frame==PlFootSnd[s->ply_id][0][6])
        {
            stair_node(h,17,&node);
            stair_footstep(h,stair_floor_sound(h,s,stair_floor_num(h,node.y),node.x,node.z),0,1);
            s->status|=0x200u;
        }
        if(frame==PlFootSnd[s->ply_id][1][6])
        {
            stair_node(h,21,&node);
            stair_footstep(h,stair_floor_sound(h,s,stair_floor_num(h,node.y),node.x,node.z),0,1);
            s->status|=0x200u;
        }
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- dnu (16) */
static void dnu(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 node;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,42598,(s->ay+8192)&~0x3FFF);
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=21;
        s->hokan_rate=42598;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=(s->ay+8192)&~0x3FFF;
        s->spd=2.0f;
        if(s->status&0x100000u)
        {
            /* bhSetEffect(108, (POINT*)&plp->px, NULL, 10): pnt ist ein POINT,
             * seine ox/oy/oz sind die drei auf px/py/pz folgenden Felder der
             * Spielerarbeit, also ax/ay/az (types.h BH_PWORK 0x1C..0x24).
             * bhSetEffect kopiert sie nach lox/loy/loz (effect.c 355-357) und
             * setzt ax/ay/az der neuen Arbeit auf 0; die Winkel werden also als
             * float-Bitmuster uebernommen, nicht als Winkel. */
            RECVX_STAIR_EFFECT effect;
            memset(&effect,0,sizeof(effect));
            effect.flg=1;
            effect.id=108;
            effect.px=s->px;effect.py=s->py;effect.pz=s->pz;
            effect.sx=effect.sy=effect.sz=1.0f;
            effect.lkono=10;
            effect.has_offset=1;
            stair_bits(s->ax,&effect.offset[0]);
            stair_bits(s->ay,&effect.offset[1]);
            stair_bits(s->az,&effect.offset[2]);
            stair_effect(h,&effect);
        }
        s->ct3=0;
        s->mode3=4;
        break;
    case 4:
        s->floor=stair_floor_num(h,9.0f+s->py);
        s->mode3=5;
        /* faellt durch */
    case 5:
        if((s->st_flg&0x40u)&&(s->status&0x100000u)&&s->ct3==0)
        {
            ++s->ct3;
            stair_footstep(h,523,3,1);
        }
        if(stair_frame(s)==15||stair_frame(s)==29)
        {
            /* plp->gpx/gpz: Spieler-Work haelt am Frameende owP[0].mtx[12/14],
             * sobald stflg 0x30 gesetzt ist (player.c 1738-1753); hier ist das
             * 0x10/0x20 dieses Ablaufs gesetzt, also Knoten 0 statt px/pz. */
            stair_node(h,0,&node);
            s->flr_snd=stair_floor_sound(h,s,s->floor,node.x,node.z);
            stair_footstep(h,s->flr_snd,1,1);
            s->status|=0x200u;
        }
        s->flg|=0x200000u;
        if(s->flg&0x400000u)
        {
            stair_node(h,0,&node);
            s->px=node.x;
            s->py+=9.0f;
            s->pz=node.z;
            s->status&=~0x80010020u;
            s->flg&=~0xD0400u;
            s->flg|=0x118u;
            stair_set_floor(h,s);
            if(h->grand)s->py=h->grand[s->floor+2];
            s->mode0=1;
            s->motion=PlMtnAct[s->wpntp][s->dmlvl][0];
            s->hokan_rate=49152;
            s->hokan_count=8;
            s->mtn_add=65536;
            s->frame=0;
            s->mtn_md|=0x100;
            s->active=0;
        }
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- dnd (17) */
static void dnd(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 node,origin;
    float top,py;
    int ay,turn,half;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,42598,(s->ay+8192)&~0x3FFF);
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=22;
        s->hokan_rate=42598;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=(s->ay+8192)&~0x3FFF;
        s->ct3=0;
        s->mode3=4;
        break;
    case 4:
        s->floor=stair_floor_num(h,s->py-9.0f);
        s->mode3=5;
        if(h->vibration)h->vibration(h->user,0,7);
        else s->unsupported_services|=RECVX_STAIR_UNBOUND_VIBRATION;
        /* faellt durch */
    case 5:
        if((s->st_flg&0x40u)&&s->ct3==0)
        {
            /* bhCheckWater((NJS_POINT3*)&plp->mlwP->owP[ply_id ? 16 : 19].mtx[12]) */
            if(h->node_position)h->node_position(h->user,s->ply_id?16u:19u,&node.x);
            if(h->water&&h->water(h->user,&node.x,&top))
            {
                /* bhSetEffect(108, (POINT*)&plp->mlwP->owP->mtx[12], NULL, 15):
                 * das Original liest den Versatz aus mtx[16]/mtx[17] und damit
                 * ueber die 4x4-Matrix hinaus; der wahre Wert ist undefiniert. */
                RECVX_STAIR_EFFECT splash;
                stair_node(h,0,&origin);
                memset(&splash,0,sizeof(splash));
                splash.flg=1;
                splash.id=108;
                splash.px=origin.x;splash.py=origin.y;splash.pz=origin.z;
                splash.sx=splash.sy=splash.sz=1.0f;
                splash.lkono=15;
                splash.has_offset=1;
                s->unsupported_services|=RECVX_STAIR_UNBOUND_EFFECT_OFF;
                stair_effect(h,&splash);
                ay=s->ay;
                py=s->py;
                s->ay=ay-16384;
                for(half=0;half<2;++half)
                {
                    RECVX_STAIR_EFFECT e;
                    unsigned node_no=half?19u:15u;
                    NJS_POINT3 centre;
                    stair_node(h,node_no,&centre);
                    for(turn=0;turn<3;++turn)
                    {
                        memset(&e,0,sizeof(e));
                        e.flg=1;
                        e.id=11;
                        e.mdlver=0;
                        e.type=1;
                        e.flr_no=0;
                        e.sx=e.sy=e.sz=3.0f;
                        e.px=centre.x;
                        e.py=top;
                        e.pz=centre.z;
                        e.ay=(turn==0)?(s->ay+16384):(turn==1)?(s->ay-16384):(s->ay+32768);
                        e.lkono=turn;
                        stair_effect(h,&e);
                    }
                    if(half==0)s->ay-=32768;
                }
                stair_footstep(h,522,3,1);
                s->py=py;
                s->ay=ay;
                ++s->ct3;
            }
        }
        if(stair_frame(s)==13||stair_frame(s)==16)
        {
            stair_node(h,0,&node);
            s->flr_snd=stair_floor_sound(h,s,s->floor,node.x,node.z);
            stair_footstep(h,s->flr_snd,1,1);
            s->status|=0x200u;
        }
        s->flg|=0x200000u;
        if(s->flg&0x400000u)
        {
            /* Das Original setzt frm_no hier erst auf 65536 und im selben
             * Block wieder auf 0; die erste Zuweisung ist wirkungslos. */
            stair_node(h,0,&node);
            s->px=node.x;
            s->py-=9.0f;
            s->pz=node.z;
            s->status&=~0x80010020u;
            s->flg&=~0xD0400u;
            s->flg|=0x118u;
            stair_set_floor(h,s);
            if(h->grand)s->py=h->grand[s->floor+2];
            s->mode0=1;
            s->motion=PlMtnAct[s->wpntp][s->dmlvl][0];
            s->hokan_rate=49152;
            s->hokan_count=8;
            s->mtn_add=65536;
            s->frame=0;
            s->mtn_md|=0x100;
            s->active=0;
        }
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- hsu (20) */
static void hsu(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 node;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,32768,-((int)(182.04445f*(s->area.prm1*90))&0xFFFF));
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=62;
        s->hokan_rate=32768;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=-((int)(182.04445f*(s->area.prm1*90))&0xFFFF);
        s->ct0=(s->area.prm2/2)-2;
        s->mode3=4;
        s->px=s->area.px+(0.5f*s->area.w);
        s->pz=s->area.pz+(0.5f*s->area.d);
        switch(s->area.prm1)
        {
        case 0:s->pz+=5.0f;break;
        case 1:s->px-=5.0f;break;
        case 2:s->pz-=5.0f;break;
        case 3:s->px+=5.0f;break;
        default:break;
        }
        break;
    case 4:
        if(stair_frame(s)==0)
        {
            stair_node(h,0,&node);
            s->px=node.x;
            s->pz=node.z;
            s->py=node.y;
            s->mode3=5;
            s->motion=63;
            s->frame=0;
        }
        break;
    case 5:
        s->flg|=0x200000u;
        if(s->flg&0x400000u)
        {
            stair_node(h,0,&node);
            s->py=node.y;
            s->frame=0;
            --s->ct0;
            if(s->ct0<=0)
            {
                s->px=node.x;
                s->pz=node.z;
                s->mode3=6;
                s->motion=64;
                s->frame=0;
            }
        }
        break;
    case 6:
        if(stair_frame(s)==0)stair_finish(h,s,s->area.py,0x80010010u,4.0f*s->area.prm2);
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- hsd (21) */
static void hsd(const RECVX_STAIR_HOST* h,RECVX_STAIR_STATE* s)
{
    NJS_POINT3 node;
    switch(s->mode3)
    {
    case 0:
        stair_open(s,h,32768,32768-((int)(182.04445f*(s->area.prm1*90))&0xFFFF));
        break;
    case 1:
    case 2:
        stair_approach(s);
        break;
    case 3:
        s->flg&=~0x4C0000u;
        s->motion=65;
        s->hokan_rate=32768;
        s->hokan_count=4;
        s->mtn_add=65536;
        s->frame=0;
        s->ay=32768-((int)(182.04445f*(s->area.prm1*90))&0xFFFF);
        s->ct0=(s->area.prm2/2)-2;
        s->mode3=4;
        s->px=s->area.px+(0.5f*s->area.w);
        s->pz=s->area.pz+(0.5f*s->area.d);
        switch(s->area.prm1)
        {
        case 0:s->pz+=0.5f;break;
        case 1:s->px-=0.5f;break;
        case 2:s->pz-=0.5f;break;
        case 3:s->px+=0.5f;break;
        default:break;
        }
        break;
    case 4:
        if(stair_frame(s)==0)
        {
            stair_node(h,0,&node);
            s->px=node.x;
            s->pz=node.z;
            s->py=node.y;
            s->mode3=5;
            s->motion=66;
            s->frame=0;
        }
        break;
    case 5:
        s->flg|=0x200000u;
        if(stair_frame(s)==0)
        {
            stair_node(h,0,&node);
            s->py=node.y;
            s->frame=65536;
            --s->ct0;
            if(s->ct0<=0)
            {
                s->px=node.x;
                s->pz=node.z;
                s->mode3=6;
                s->motion=67;
                s->frame=0;
            }
        }
        break;
    case 6:
        if(stair_frame(s)==0)stair_finish(h,s,s->area.py,0x80010010u,-(4.0f*s->area.prm2));
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------- Einsprung/Ablauf */

int recvx_stairs_begin(RECVX_STAIR_STATE* state,const RECVX_STAIR_AREA* area,int action,const float* grand,float px,float py,float pz,int ay,int floor,int dmlvl,int wpntp)
{
    unsigned flg,status,flg2,st_flg;
    int mtn_tp,ply_id;
    float rtspd,arp,spd;
    (void)grand;
    if(!state||!area)return 0;
    /* Felder, die der Host besitzt, ueber den Neuanfang hinweg erhalten. */
    flg=state->flg;status=state->status;flg2=state->flg2;st_flg=state->st_flg;
    mtn_tp=state->mtn_tp;ply_id=state->ply_id;
    rtspd=state->rtspd;arp=state->arp;spd=state->spd;
    memset(state,0,sizeof(*state));
    state->flg=flg;state->status=status;state->flg2=flg2;state->st_flg=st_flg;
    state->mtn_tp=mtn_tp;state->ply_id=ply_id;
    state->rtspd=rtspd;state->arp=arp;state->spd=spd;
    state->area=*area;
    state->area_valid=1;
    state->mode0=1;
    state->mode2=action;
    state->mode3=0;
    state->px=px;state->py=py;state->pz=pz;
    state->ay=ay;
    state->kdn_fr=floor;
    state->floor=floor;
    state->floor_b=floor;
    state->wpntp=wpntp;
    state->dmlvl=dmlvl;
    /* hitchk.c case 1 / case 2 / danf-Zweig setzen dieselben Arbeitsschalter
     * bis auf die Bitgruppe: case 1 (4682) setzt 0x80010010, case 2 (4750)
     * und der danf-Zweig (4949) setzen 0x80010020. player.c raeumt genau
     * diese Gruppe wieder ab (3107/3276/4101/4296 gegenueber 3482/3702),
     * also gehoert sie zur gewaehlten Aktion 16/17. Die Etage selbst hat der
     * Aufrufer bereits mit bhCheckFloorNum (py - 2.0f * prm2) bestimmt, wie
     * das Original an der Auswahlstelle. */
    state->status|=(action==16||action==17)?0x80010020u:0x80010010u;
    state->flg|=0x10400u;
    state->flg&=~0x110u;
    /* hitchk.c case 1 (4686) und case 2 (4757) setzen sys->st_flg 0x4, die
     * danf-Sonde (4952) nicht; nur der Danf-Zweig laesst das Bit stehen. */
    if(action!=17)state->st_flg|=0x4u;
    /* RECVX_STAIR_UNBOUND_HEADWORK wird nicht mehr vorbelegt: der mode3-0-Block
     * ruft release_head_look und meldet das Bit nur, wenn der Host den Dienst
     * nicht stellt.  Hokan und mtn_attr bleiben offen. */
    state->unsupported_services=RECVX_STAIR_UNBOUND_HOKAN|RECVX_STAIR_UNBOUND_MTN_ATTR;
    state->active=1;
    return 1;
}

int recvx_stairs_tick(RECVX_STAIR_STATE* state,const RECVX_STAIR_HOST* host)
{
    unsigned frm_num;
    int total,sample;
    if(!state||!host||!state->active)return 0;
    state->st_flg|=0x4u;
    switch(state->mode2)
    {
    case 14:kdu(host,state);break;
    case 15:kdd(host,state);break;
    case 16:dnu(host,state);break;
    case 17:dnd(host,state);break;
    case 20:hsu(host,state);break;
    case 21:hsd(host,state);break;
    default:
        /* Kein Zweig des Originals; die Laufzeit uebergibt nur 14..21. */
        state->active=0;
        return 0;
    }
    if(!state->active)
    {
        /* Der Abschlussblock hat die Ruhemotion mit frm_no 0 aufgesetzt; das
         * Original tastet sie im selben Durchlauf noch ab und zeichnet sie. */
        state->sample_frame=state->frame;
        return 0;
    }
    /* bhSetMotion: die Aktionsroutine ist gelaufen, jetzt der Bildvorlauf und
     * die Berechnung von plp->flg 0x400000 fuer den naechsten Durchlauf. */
    frm_num=host->motion_frames?host->motion_frames(host->user,state->motion):0u;
    if(frm_num==0)
    {
        state->unsupported_services|=RECVX_STAIR_UNBOUND_MOTION;
        return 1;
    }
    /* SetMtn* tastet objP am aktuellen plp->frm_no ab und schiebt erst danach
     * vor (Motion.c:70-108); diese Bildnummer gehoert zur jetzt gueltigen
     * Pose und damit zu bhCalcFixOffset und zum gezeichneten Bild. */
    sample=state->frame;
    total=(int)(frm_num<<16);
    if((state->mtn_md&0x1)&&state->frm_mode!=0)state->frame-=state->mtn_add;
    else state->frame+=state->mtn_add;
    if(state->mtn_md&0x1)
    {
        if(state->frame<=0||state->frame>=total-65536)state->frm_mode=~state->frm_mode;
    }
    else
    {
        if(state->frame<0)state->frame+=total;
        else if(state->frame>=total)state->frame-=total;
    }
    if(stair_frame(state)>=(int)frm_num-1)state->flg|=0x400000u;
    else state->flg&=~0x400000u;
    /* player.c:1538-1558: die Fussfixierung laeuft nach dem Bildvorschub.
     * plp->flg 0x80000 waehlt das Bein, 0x8000000 den XYZ-Zweig. */
    if(state->flg&0x40000u)stair_fix_position(host,state,sample,(state->flg&0x8000000u)!=0);
    state->sample_frame=sample;
    /* bhCalcModel (player.c:1684) am Ende des Durchlaufs: owP gehoert ab jetzt
     * zu dieser Pose und dieser Lage. */
    state->draw_motion=state->motion;
    state->draw_frame=sample;
    state->draw_px=state->px;state->draw_py=state->py;state->draw_pz=state->pz;
    state->draw_ax=state->ax;state->draw_ay=state->ay;state->draw_az=state->az;
    state->draw_valid=1;
    return 1;
}
