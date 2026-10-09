#include "player_push_core.h"
#include <stdlib.h>
/* PORTIERUNGSLÜCKE (Policy 2026-10-09): Diese Datei band bhCheckWall,
 * bhCheckWallType, bhCheckFloorP, bhCheckDansa, bhObj001, bhCPM2_act_psh,
 * PushPlayerCollision, PushRotationSpeed, bhSetMotion und die PlyInfo-Tabelle
 * ein, die tools/generate_player_push.py als player_push.inc aus player.c,
 * objitm.c, hitchk.c, Motion.c, pwksub.c und ps2_NaColi.c erzeugt hat.
 * Generator und Include liegen in archived/FORBIDDEN_DONTUSE/ und dürfen
 * weder gelesen noch ausgeführt werden. Mit dem Include ist auch der hier
 * gehaltene eigene Spieler-/System-/Raum-Arbeitssatz (BH_PWORK, PUSH_SYS,
 * PUSH_ROOM, O_WRK-Effektpool) entfallen, den die Policy zusätzlich als
 * separaten Work-Owner verbietet.
 *
 * Zu leisten (jeweils als eigene native Mechanik, nicht aus einem Generator
 * über die Originalquellen):
 *  - die Spielerkollision gegen die ATR-Wandtabelle einschließlich der
 *    radiuserweiterten Boxflächen, der Sonderformen (z. B. Typ 7 als Ebene),
 *    der Attributfilter und des Trefferbits in plp->stflg,
 *  - die Bodenermittlung bhCheckFloorP mit rom->grand und den beweglichen
 *    Flächen, sowie bhCheckDansa mit seinen temporären Stufenwänden,
 *  - bhObj001: die Objektzustände der schiebbaren Kisten samt der
 *    nachgeführten dynamischen ETC-/Boden-Einträge in sys->metcp/mflrp,
 *  - bhCPM2_act_psh: Kontaktzähler, MTN49/50-Ablauf, Rücklauf und Freigabe,
 *  - die Attributaktion 0x40 (bhSetEffect 29) an der Wand.
 *
 * Solange das offen ist, melden alle Abfragen den dokumentierten Neutralwert:
 * kein Treffer, keine bewegliche Wand/Stufe/Bodenfläche, keine Schiebeaktion
 * (active bleibt 0) und unsupported_services bleibt 0, weil keiner der
 * fehlenden Dienste angefragt wird. Die Kollisionsaufrufe geben genau die vom
 * Aufrufer vorgeschlagene Zielposition zurück — es wird also keine Auflösung
 * vorgenommen, statt eine zu erfinden. Objektpositionen werden nur als
 * Buchführung über die vom Host gelieferten Werte gehalten; es wird keine
 * Objektbewegung simuliert.
 *
 * Sichtbare Folge: Der Spieler hat keine Kollision mehr. Er läuft durch
 * Wände, Türen und Möbel, fällt nicht auf die richtige Etage und steht nicht
 * auf Kisten; Stufenkanten existieren nicht. Schiebbare Kisten lassen sich
 * nicht mehr anfassen oder bewegen, und der Schiebeablauf (MTN49/50) startet
 * nie. Kein geratener Ersatzwert und keine nachgebaute Kollisionsauflösung. */

struct RECVX_PUSH_CORE {
    RECVX_PUSH_HOST host;
    unsigned flags;        /* sp_flg des Hosts, nur gespeichert */
    unsigned object_count;
    float* object_position; /* 3 Werte je Objekt, reine Host-Buchführung */
};

/* Leere, aber gültige Rückgabetabelle: der Aufrufer bildet daraus einen
 * Bereich der Länge 0. */
static const RECVX_STAIR_AREA recvx_push_no_areas[1];

static void recvx_push_neutral_state(RECVX_PUSH_STATE* state,float x,float y,float z,int ay,unsigned motion,int frame){
    if(!state)return;
    state->active=0;state->phase=0;state->contact=0;state->wall=0;
    state->ay=ay&65535;state->motion=(int)motion;state->frame=frame;state->mode0=0;
    state->flags=0;state->status=0;state->unsupported_services=0;
    state->x=x;state->y=y;state->z=z;state->speed=0;
}

RECVX_PUSH_CORE* recvx_push_create(const RECVX_STAIR_AREA* walls,unsigned n,float height,const float* grand,const RECVX_PUSH_OBJECT* objects,unsigned count,const RECVX_PUSH_HOST* host){
    RECVX_PUSH_CORE* core=calloc(1,sizeof *core);
    (void)walls;(void)n;(void)height;(void)grand;
    if(!core)return NULL;
    if(host)core->host=*host;
    core->flags=5;
    core->object_count=count;
    core->object_position=calloc(count?count*3:3,sizeof(float));
    if(!core->object_position){free(core);return NULL;}
    if(objects)for(unsigned i=0;i<count;++i){
        core->object_position[i*3+0]=objects[i].x;
        core->object_position[i*3+1]=objects[i].y;
        core->object_position[i*3+2]=objects[i].z;
    }
    return core;
}
void recvx_push_destroy(RECVX_PUSH_CORE* core){
    if(!core)return;
    free(core->object_position);
    free(core);
}
void recvx_push_enabled(RECVX_PUSH_CORE* core,unsigned flags){
    if(core)core->flags=flags;
}
void recvx_push_motions(RECVX_PUSH_CORE* core,RECVX_PUSH_MOTION* motions,unsigned count,const float* translation,unsigned translation_count,RECVX_PUSH_POSE* poses,unsigned nodes){
    (void)core;(void)motions;(void)count;(void)translation;(void)translation_count;(void)poses;(void)nodes;
}
void recvx_push_terrain(RECVX_PUSH_CORE* core,const RECVX_STAIR_AREA* etc,unsigned etc_count,const RECVX_STAIR_AREA* floors,unsigned floor_count){
    (void)core;(void)etc;(void)etc_count;(void)floors;(void)floor_count;
}
void recvx_push_contact(RECVX_PUSH_CORE* core,RECVX_PUSH_STATE* state,float ox,float oy,float oz,float x,float y,float z,int ay,int floor,int walking,int dmlvl,int wpntp){
    (void)core;(void)ox;(void)oy;(void)oz;(void)floor;(void)walking;(void)dmlvl;(void)wpntp;
    /* Keine Kollisionsauflösung: die vorgeschlagene Zielposition bleibt stehen. */
    recvx_push_neutral_state(state,x,y,z,ay,0,0);
}
void recvx_push_wall_resolve(RECVX_PUSH_CORE* core,RECVX_PUSH_STATE* state,float x,float y,float z,int ay,int floor,float radius,float height){
    (void)core;(void)floor;(void)radius;(void)height;
    recvx_push_neutral_state(state,x,y,z,ay,0,0);
}
void recvx_push_tick(RECVX_PUSH_CORE* core,RECVX_PUSH_STATE* state,int forward,int ply_id,int dmlvl,int wpntp){
    (void)core;(void)forward;(void)ply_id;(void)dmlvl;(void)wpntp;
    if(!state)return;
    recvx_push_neutral_state(state,state->x,state->y,state->z,state->ay,(unsigned)state->motion,state->frame);
}
void recvx_push_object(RECVX_PUSH_CORE* core,unsigned index,float out[3]){
    out[0]=out[1]=out[2]=0;
    if(!core||index>=core->object_count)return;
    out[0]=core->object_position[index*3+0];
    out[1]=core->object_position[index*3+1];
    out[2]=core->object_position[index*3+2];
}
void recvx_push_update_object(RECVX_PUSH_CORE* core,unsigned index,float x,float y,float z,int hidden){
    (void)hidden;
    if(!core||index>=core->object_count)return;
    core->object_position[index*3+0]=x;
    core->object_position[index*3+1]=y;
    core->object_position[index*3+2]=z;
}
void recvx_push_update_walls(RECVX_PUSH_CORE* core,const RECVX_STAIR_AREA* walls,unsigned n){
    (void)core;(void)walls;(void)n;
}
unsigned recvx_push_walls(RECVX_PUSH_CORE* core,const RECVX_STAIR_AREA** out){
    (void)core;*out=recvx_push_no_areas;return 0;
}
unsigned recvx_push_steps(RECVX_PUSH_CORE* core,const RECVX_STAIR_AREA** out){
    (void)core;*out=recvx_push_no_areas;return 0;
}
unsigned recvx_push_floors(RECVX_PUSH_CORE* core,const RECVX_STAIR_AREA** out){
    (void)core;*out=recvx_push_no_areas;return 0;
}
int recvx_push_probe(const RECVX_STAIR_AREA* walls,unsigned n,float height,float x,float y,float z,float radius,float body_height,unsigned flags,unsigned* attr){
    (void)walls;(void)n;(void)height;(void)x;(void)y;(void)z;(void)radius;(void)body_height;(void)flags;(void)attr;
    return 0; /* kein Treffer; `attr` bleibt unberührt */
}
