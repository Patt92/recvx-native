#include "en01_damage.h"
#include <string.h>
/* PORTIERUNGSLUECKE (Policy 2026-10-09): Der komplette Gegnerkern dieser Datei
   kam aus en01_damage.inc und en04_damage.inc (Typen in
   en01_damage_types.inc), erzeugt von tools/generate_en01_damage.py.
   Generator und Includes sind Quarantaene (archived/FORBIDDEN_DONTUSE/) und
   duerfen weder gelesen noch ausgefuehrt werden. Damit entfallen:
   - zonzon*.c / en01.c: bhEne01_Init, bhEne01_DmgCheck, bhEne01_Damage,
     bhEne01_Die, bhEne01_Move mit MV00..MV05, Brain/Search/ActionModeCheck,
     bhEne01_Nage/EatCheck/PlyDG/PlayerLink, bhEne01_SetMtn/CheckMtnTbl,
     bhEne01_RotNeck, bhEne01_SePlay, bhBombHit, bhCheckCapCol2Capsule,
     bhEne_InitDamage, bhEff_Sub356,
   - en04.c: bhEne04_Init, bhEne04_DmgChk, bhEne04_DG00/DG01, bhEne04_DD00,
     bhEne04_MV04/MV05,
   - eneset.c: bhEne00 (Sprengfass),
   - und alle Originalstrukturen (BH_PWORK, ML_WORK, O_WORK, MN_WORK, ATR_WORK,
     EF_WORK, BT_WORK, BP_WORK, EXP0-Zugriffsmakros).

   Mit den Prozeduren entfaellt auch die Host-Bruecke, die nur fuer sie
   existierte: das eigene sys/rom-Teilwerk, die Gelaende- und Routentabellen,
   der eff[512]-Slotpool, die Wand-/Linien-/Partikeldienste und - von der
   Policy ausdruecklich verboten - die eigene BH_PWORK-Instanz fuer Spieler
   (plp) und Gegnerliste (ene), also ein zweiter Besitzer desselben Werks.

   Zu leisten: Gegner-KI, Trefferreaktion, Tod, Greifen/Beissen, Spielerschaden
   und das Sprengfass als eigene native Mechanismen bestimmen - nicht ueber
   einen Generator auf die Originalquellen.

   Solange das offen ist, fuehrt kein Tick eine Originalroutine aus. Die
   Werkspeicher halten nur noch die Werte, die der Host selbst hineinschreibt
   (Position, Winkel, HP, Motion, Modi, Typ), und recvx_en01_state spiegelt
   genau diese zurueck. Das ist keine ersetzte Gegnerlogik, sondern reine
   Durchleitung: ohne sie wuerde der Aufrufer die vom Raum gelieferten Werte
   mit Nullen ueberschreiben und alle Gegner in den Ursprung versetzen. Alles
   andere bleibt null: flg 0 (also kein aktives, zielbares, getroffenes oder
   totes Werk), kein neck_angle, kein flinch, kein mdflg, keine
   Effektanforderung, keine Stimme. recvx_en01_capsule_hit meldet -1 (kein
   Gelenk getroffen), recvx_en00_wall meldet 0 (keine bewegliche Wand).

   Sichtbare Folge: Gegner sind voellig inert. Sie verfolgen den Spieler nicht,
   greifen nicht, nehmen keinen Schaden, taumeln nicht, sterben nicht und sind
   nicht anvisierbar; Treffer und Explosionen bleiben ohne Wirkung. Zombies
   stehen in der Pose, die der Host aus den Raumdaten setzt (ohne
   bhEne01_Init gibt es keine Startmotion, also Motion 0 bzw. Bindepose).
   Hunde (en04) verhalten sich genauso. Das Sprengfass (en00) stellt keine
   Wand, explodiert nicht und verschwindet nicht. */

/* Host-Spiegel: ausschliesslich Werte, die der Aufrufer selbst setzt. */
typedef struct {
    float px,py,pz;
    int ay,hp,enemy_no;
    unsigned type,mdlver,mtn_no;
    int frm_no,mtn_add;
    int mode0,mode1,mode2,mode3;
} EN01_STORE;
_Static_assert(sizeof(EN01_STORE)<=sizeof(RECVX_EN01),"RECVX_EN01 storage too small");

static EN01_STORE* store(RECVX_EN01* work){return (EN01_STORE*)work;}

void recvx_en01_set_host(const RECVX_EN01_HOST* host){(void)host;}
void recvx_en01_set_frame_counts(const unsigned* counts,unsigned count){(void)counts;(void)count;}
void recvx_en04_set_frame_counts(const unsigned* counts,unsigned count){(void)counts;(void)count;}
void recvx_en01_set_neighbours(const RECVX_EN01* const* list,unsigned count){(void)list;(void)count;}
void recvx_en01_set_ai(int enabled){(void)enabled;}
void recvx_en01_set_routes(const unsigned char* records,unsigned count,const unsigned char* table){(void)records;(void)count;(void)table;}
void recvx_en01_set_terrain(const unsigned char* etc,unsigned etc_count,const unsigned char* flr,unsigned flr_count){(void)etc;(void)etc_count;(void)flr;(void)flr_count;}
void recvx_en01_set_mobile_terrain(const unsigned char* etc,unsigned etc_count,const unsigned char* flr,unsigned flr_count){(void)etc;(void)etc_count;(void)flr;(void)flr_count;}
void recvx_en01_set_player(const RECVX_EN01_PLAYER* player){(void)player;}
void recvx_en01_player_tick(void){}
void recvx_en01_player_reset(void){}
int recvx_en01_player_state(RECVX_EN01_PLAYER_STATE* out){
    /* No player-state owner is bound here after removal of the separate
       original work. Report absence instead of inventing HP 0, which the
       world previously consumed as damage every tick. Do not introduce a
       second player store to supply a plausible replacement state. */
    (void)out;return 0;
}

void recvx_en01_init(RECVX_EN01* work,unsigned type,unsigned mdlver,int hard,unsigned joints,int flr_no,float px,float py,float pz,int ay,const RECVX_EN01* const* previous,unsigned previous_count){
    EN01_STORE* s;
    (void)hard;(void)joints;(void)flr_no;(void)previous;(void)previous_count;
    if(!work)return;
    s=store(work);memset(s,0,sizeof *s);
    s->type=type;s->mdlver=mdlver;s->px=px;s->py=py;s->pz=pz;s->ay=ay;
}
void recvx_en04_init(RECVX_EN01* work,unsigned type,int hard,int floor,float x,float y,float z,int ay){
    EN01_STORE* s;
    (void)hard;(void)floor;
    if(!work)return;
    s=store(work);memset(s,0,sizeof *s);
    s->type=type;s->px=x;s->py=y;s->pz=z;s->ay=ay;
}
void recvx_en01_set_joints(RECVX_EN01* work,const float* matrices,unsigned count){(void)work;(void)matrices;(void)count;}
void recvx_en01_set_enemy_no(RECVX_EN01* work,int number){if(work)store(work)->enemy_no=number;}
void recvx_en01_set_hp(RECVX_EN01* work,int hp){if(work)store(work)->hp=hp;}
void recvx_en01_set_transform(RECVX_EN01* work,float px,float py,float pz,int ay){
    EN01_STORE* s;if(!work)return;s=store(work);s->px=px;s->py=py;s->pz=pz;s->ay=ay;
}
int recvx_en01_capsule_hit(RECVX_EN01* work,const float start[3],const float end[3],float radius,float hit[3]){
    (void)work;(void)start;(void)end;(void)radius;(void)hit;return -1;
}
void recvx_en01_hit(RECVX_EN01* work,int weapon,int joint,int damage,unsigned comb_flg_bits,const float* position,const float* direction){
    (void)work;(void)weapon;(void)joint;(void)damage;(void)comb_flg_bits;(void)position;(void)direction;
}
void recvx_en01_bomb(RECVX_EN01* work,const float* position,float radius,int damage_max,int damage_min,const float* contact,float distance){
    (void)work;(void)position;(void)radius;(void)damage_max;(void)damage_min;(void)contact;(void)distance;
}
void recvx_en01_tick(RECVX_EN01* work){(void)work;}
void recvx_en04_tick(RECVX_EN01* work){(void)work;}
void recvx_en01_state(const RECVX_EN01* work,RECVX_EN01_STATE* out){
    const EN01_STORE* s;
    if(!out)return;
    memset(out,0,sizeof *out);
    /* Ohne Werk ist kein Gegner zielbar (im Original flg & 8). */
    out->untargetable=1;
    if(!work)return;
    s=(const EN01_STORE*)work;
    out->hp=s->hp;out->type=s->type;out->mdlver=s->mdlver;out->ay=s->ay;
    out->px=s->px;out->py=s->py;out->pz=s->pz;
    out->mtn_no=s->mtn_no;out->frm_no=s->frm_no;out->pose_frame=s->frm_no;out->mtn_add=s->mtn_add;
    out->mode0=s->mode0;out->mode1=s->mode1;out->mode2=s->mode2;out->mode3=s->mode3;
}
void recvx_enemy_motion_mode(RECVX_EN01* work,unsigned mode3){if(work)store(work)->mode3=(int)mode3;}
void recvx_enemy_modes(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type){
    EN01_STORE* s;if(!work)return;s=store(work);
    s->mode0=mode0;if(mode1>=0)s->mode1=mode1;s->mode2=mode2;if(mode3>=0)s->mode3=mode3;
    if(type!=~0u)s->type=(s->type&0xff00u)|(type&0xffu);
}
void recvx_enemy_release(RECVX_EN01* work,int mode0,int mode1,int mode2,int mode3,unsigned type){
    recvx_enemy_modes(work,mode0,mode1,mode2,mode3,type);
}
void recvx_enemy_motion(RECVX_EN01* work,unsigned motion,int frame,int add){
    EN01_STORE* s;if(!work)return;s=store(work);s->mtn_no=motion;s->frm_no=frame;s->mtn_add=add;
}
void recvx_en00_reset(const float* ground,unsigned count){(void)ground;(void)count;}
void recvx_en00_init(RECVX_EN01* work,int flr_no,float px,float py,float pz,int ay){
    EN01_STORE* s;
    (void)flr_no;
    if(!work)return;
    s=store(work);memset(s,0,sizeof *s);s->px=px;s->py=py;s->pz=pz;s->ay=ay;
}
void recvx_en00_tick(RECVX_EN01* work){(void)work;}
int recvx_en00_wall(const RECVX_EN01* work,RECVX_EN00_WALL* out){(void)work;(void)out;return 0;}
