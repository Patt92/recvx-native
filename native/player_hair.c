/* Haarkette des angehaengten Spielerobjekts obwp[2] (Auftrag B04).
 *
 * Neu geschriebener nativer Code nach den in player_hair.h genannten
 * Originalroutinen.  Es wurde kein Funktionskoerper uebernommen und kein
 * Generator benutzt; die Quarantaene wurde weder gelesen noch verwendet.
 *
 * Aufbau des Originalverfahrens, wie es hier umgesetzt ist:
 *
 *   bhCalcHair (player.c:7207) bildet je Bild die Summe der lokalen Winkel der
 *   Spielerknoten 0..5 plus pp->ax/ay/az, merkt die Wurzel des Vorbildes und
 *   rechnet die Wurzelverschiebung der XZ-Ebene durch die Umkehrdrehung
 *   (-ay,-ax,-az) in den Kopfraum.  Das Ergebnis spx/spz wird auf +-1
 *   begrenzt; spy ist die rohe Hoehendifferenz, ebenfalls begrenzt.  spz wird
 *   von njCalcPoint geschrieben und danach begrenzt.
 *
 *   bhObjClpn (objitm.c:2122) traegt diese Geschwindigkeit mit -0.25 skaliert
 *   in einen 128 Eintraege langen Ringpuffer und glaettet sie mit Faktor
 *   0.333.  Die vier Segmente lesen den Puffer je drei Bilder weiter zurueck
 *   (ct0 - 3*i), drehen den Wert um hair->ay in den Weltraum und haengen ihn
 *   an die Position des Vorgaengers.  Zwei Mindestabstaende halten die Kette
 *   vom Kopf weg (1.0 um owP[5]*(0,1,0), 1.3 um owP[5]*(0,1.8,-0.3)), zwei
 *   Hoechstabstaende begrenzen jedes Segment auf 0.5 zum Vorgaenger.  Aus dem
 *   Restvektor entstehen ueber atan2 die beiden Knotenwinkel; rx/ry nehmen die
 *   bereits aufgebrachte Drehung der Kette wieder heraus.
 */
#include "player_hair.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <math.h>
#include <string.h>

/* objitm.c:2210-2216: Anteil der neuen Geschwindigkeit und der Glaettung. */
#define HAIR_SPEED_SCALE 0.25f
#define HAIR_FILTER      0.333f
/* objitm.c:2234: Abstand der Segmente im Ringpuffer. */
#define HAIR_SEGMENT_LAG 3
/* objitm.c:2255-2262: Grenzen des Fallanteils. */
#define HAIR_FALL_LIMIT  0.5f
/* objitm.c:2299/2340: Hoechstabstand eines Segments zum Vorgaenger. */
#define HAIR_SEGMENT_REACH 0.5f
/* objitm.c:2281/2326: Mindestabstaende der Kette zum Kopf. */
#define HAIR_HEAD_CLEARANCE  1.0f
#define HAIR_SHOULDER_CLEARANCE 1.3f
/* objitm.c:2360/2365: Umrechnungen in den Winkelbereich des Originals. */
#define HAIR_ANGLE_FROM_ATAN 10430.381f
#define HAIR_ANGLE_PER_DEGREE 182.04445f
#define HAIR_PITCH_GAIN 150.0f
/* objitm.c:2360: Vorspann des Nenners der Gierbildung. */
#define HAIR_YAW_BIAS 0.1f

static void hair_clamp(float* value,float limit){
    if(*value> limit)*value= limit;
    if(*value<-limit)*value=-limit;
}

/* bhCalcHair, player.c:7207-7298. */
static void hair_calc(RECVX_HAIR_STATE* s,const RECVX_HAIR_INPUT* in,const float root[3]){
    RECVX_HAIR_WORK* h=&s->hair;
    NJS_MATRIX m;NJS_POINT3 ps;
    int32_t ax,ay,az;
    unsigned i;

    h->axb=h->ax;h->ayb=h->ay;h->azb=h->az;

    /* Die Summe laeuft im Original ueber short, also mit Ueberlauf bei 16 Bit
     * (player.c:7211 deklariert ax/ay/az als short). */
    ax=in->ax;ay=in->ay;az=in->az;
    for(i=0;i<6;++i){ax+=in->angles[i][0];ay+=in->angles[i][1];az+=in->angles[i][2];}
    h->ax=(int16_t)ax;h->ay=(int16_t)ay;h->az=(int16_t)az;

    h->oxb=h->ox;h->oyb=h->oy;h->ozb=h->oz;
    h->ox=root[0];h->oy=root[1];h->oz=root[2];

    ps.x=h->ox-h->oxb;
    ps.y=0;
    ps.z=h->oz-h->ozb;

    njUnitMatrix(&m);
    njRotateY(&m,-h->ay);
    njRotateX(&m,-h->ax);
    njRotateZ(&m,-h->az);
    /* Das Original schreibt nach &hair->spx, also spx/spy/spz in Folge; spy
     * wird unmittelbar danach durch die rohe Hoehendifferenz ersetzt. */
    njCalcPoint(&m,&ps,(NJS_POINT3*)&h->spx);

    h->spy=h->oy-h->oyb;

    hair_clamp(&h->spx,1.0f);
    hair_clamp(&h->spy,1.0f);
    hair_clamp(&h->spz,1.0f);
}

/* Anfangszustand des Verlaufs: bhObjClpn case 0 (objitm.c:2158-2177) und der
 * gleichlautende Block des Resets (objitm.c:2188-2203). */
static void hair_clear_history(RECVX_HAIR_STATE* s){
    memset(s->history,0,sizeof s->history);
    memset(s->goal,0,sizeof s->goal);
    memset(s->point,0,sizeof s->point);
    s->xn=s->yn=s->zn=0;
    s->spd=0;
}

RECVX_HAIR_VARIANT recvx_hair_begin(RECVX_HAIR_STATE* s,int ply_id){
    /* bhSetObject (objitm.c:157) nullt das gesamte Objektwerk; flg kommt aus
     * dem lkmtab-Eintrag (player.c:24-25: 129 = 0x81, also 0x80 angehaengt
     * und 0x1 bewegt). */
    memset(s,0,sizeof *s);
    s->flg=0x81u;
    s->mode0=0;
    /* bhSetPlayer/bhResetPlayer binden obwp[2] nur fuer diese beiden Figuren
     * (player.c:756-785, 962-991). */
    if(ply_id==0)return RECVX_HAIR_VARIANT_CLPN;
    if(ply_id==3)return RECVX_HAIR_VARIANT_WSSG;
    return RECVX_HAIR_VARIANT_NONE;
}

int recvx_hair_tick(RECVX_HAIR_STATE* s,RECVX_HAIR_VARIANT variant,
                    const RECVX_HAIR_INPUT* in,int32_t angles[RECVX_HAIR_SEGMENTS][2]){
    RECVX_HAIR_WORK* h=&s->hair;
    NJS_MATRIX head,m;
    NJS_POINT3 local,root,ps1,ps2,ps3,ps4,ps5;
    float px,py,pz,ln;
    /* objitm.c:2127-2128 deklariert ax/ay und rx/ry als short: die Winkel
     * werden auf 16 Bit gekuerzt, bevor rx/ry sie zuruecknehmen. */
    int16_t rx,ry,ax,ay;
    unsigned i;

    for(i=0;i<RECVX_HAIR_SEGMENTS;++i){angles[i][0]=0;angles[i][1]=0;}

    /* bhObjWssg (objitm.c:2386) hat einen leeren Koerper: id 1212 trifft im
     * Verteiler bhJumpObject2 den Index 12 und laeuft damit ohne Kette.  Das
     * ist der Originalzustand von Spieler 3, keine Luecke dieses Ports. */
    if(variant!=RECVX_HAIR_VARIANT_CLPN)return 0;

    /* objitm.c:2135: der Task laeuft nur fuer ein angehaengtes Objekt und
     * nicht im Haltezustand. */
    if(!(s->flg&0x80u)||s->mode0==2)return 0;

    /* bhControlObjItm (objitm.c:336) setzt die Objektposition vor dem Task aus
     * der Kopfmatrix und dem lokalen Versatz; bhCalcModel (MdlPut.c:244-246)
     * bildet daraus dieselbe Translation fuer owP[0].mtx, aus der bhCalcHair
     * und bhObjClpn die Wurzel lesen. */
    memcpy(head,in->head,sizeof head);
    local.x=in->offset[0];local.y=in->offset[1];local.z=in->offset[2];
    njCalcPoint(&head,&local,&root);

    if(s->mode0==0){
        /* objitm.c:2139-2178 */
        s->flg|=0x1000u;
        if(!(s->flg&0x100000u))s->flg|=0x100000u; /* die Puffer liegen hier im Zustand */
        s->ct0=0;
        hair_clear_history(s);
        s->mode0=1;
        return 0;
    }

    /* objitm.c:2184-2204: Raumwechsel/Versetzen loescht Verlauf und HAIR_WORK.
     * Das Flag wird vom Aufrufer gefuehrt (plp->flg2 0x2, event.c:6026). */
    if(in->reset){
        memset(h,0,sizeof *h);
        hair_clear_history(s);
    }

    hair_calc(s,in,(const float*)&root.x);

    /* objitm.c:2208-2224: der neue Eintrag des Ringpuffers ist die geglaettete,
     * gegengerichtete Wurzelgeschwindigkeit. */
    {
        const float tx=-h->spx*HAIR_SPEED_SCALE;
        const float ty=-h->spy*HAIR_SPEED_SCALE;
        const float tz=-h->spz*HAIR_SPEED_SCALE;
        s->xn+=HAIR_FILTER*(tx-s->xn);
        s->yn+=HAIR_FILTER*(ty-s->yn);
        s->zn+=HAIR_FILTER*(tz-s->zn);
    }
    s->history[s->ct0][0]=s->xn;
    s->history[s->ct0][1]=s->yn;
    s->history[s->ct0][2]=s->zn;

    /* objitm.c:2218-2220: die Kette haengt an der Objektwurzel. */
    px=root.x;py=root.y;pz=root.z;

    rx=0;ry=0;
    for(i=0;i<RECVX_HAIR_SEGMENTS;++i){
        const unsigned slot=(unsigned)((s->ct0-HAIR_SEGMENT_LAG*(int)i)&(RECVX_HAIR_HISTORY-1));
        float* g=s->goal[i];
        float* p=s->point[i];
        NJS_POINT3 past;

        past.x=s->history[slot][0];
        past.y=s->history[slot][1];
        past.z=s->history[slot][2];

        /* objitm.c:2236-2240: der gespeicherte Wert liegt im Kopfraum und wird
         * mit der Giersumme in den Weltraum gedreht. */
        njUnitMatrix(&m);
        njRotateY(&m,h->ay);
        njCalcPoint(&m,&past,&ps1);

        if(in->ground){
            /* objitm.c:2242-2247.  Dieses Flag (pp->flg2 0x100) wird im
             * Original von keiner Stelle gesetzt; der Zweig ist hier der
             * Vollstaendigkeit wegen gebunden. */
            g[0]=px;g[1]=py-1.0f;g[2]=pz;
        }else{
            /* objitm.c:2250-2266 */
            g[0]=px+ps1.x;
            s->spd+=past.y;
            s->spd-=HAIR_FILTER;
            hair_clamp(&s->spd,HAIR_FALL_LIMIT);
            g[1]=s->spd+(py+ps1.y);
            g[2]=pz+ps1.z;
        }

        /* objitm.c:2269-2289: Mindestabstand 1.0 zum Kopfpunkt owP[5]*(0,1,0),
         * nur in der XZ-Ebene. */
        ps2.x=0;ps2.y=1.0f;ps2.z=0;
        njCalcPoint(&head,&ps2,&ps3);
        ps2.x=g[0]-ps3.x;ps2.y=g[1]-ps3.y;ps2.z=g[2]-ps3.z;
        ln=njSqrt(ps2.x*ps2.x+ps2.z*ps2.z);
        if(ln<HAIR_HEAD_CLEARANCE){
            ps2.y=0;
            njUnitVector(&ps2);
            g[0]=ps3.x+ps2.x;
            g[2]=ps3.z+ps2.z;
        }

        /* objitm.c:2291-2293 */
        p[0]+=HAIR_FILTER*(g[0]-p[0]);
        p[1]+=HAIR_FILTER*(g[1]-p[1]);
        p[2]+=HAIR_FILTER*(g[2]-p[2]);

        /* objitm.c:2295-2310: Hoechstabstand 0.5 zum Vorgaenger. */
        ps1.x=p[0]-px;ps1.y=p[1]-py;ps1.z=p[2]-pz;
        if(njScalor(&ps1)>HAIR_SEGMENT_REACH){
            njUnitVector(&ps1);
            ps1.x*=HAIR_SEGMENT_REACH;ps1.y*=HAIR_SEGMENT_REACH;ps1.z*=HAIR_SEGMENT_REACH;
            p[0]=px+ps1.x;p[1]=py+ps1.y;p[2]=pz+ps1.z;
        }

        /* objitm.c:2312-2334: Mindestabstand 1.3 zum Schulterpunkt
         * owP[5]*(0,1.8,-0.3), dessen Hoehe aus dem Kopfpunkt uebernommen
         * wird; wieder nur in der XZ-Ebene. */
        ps4.x=0;ps4.y=1.8f;ps4.z=-0.3f;
        njCalcPoint(&head,&ps4,&ps5);
        ps5.y=ps3.y;
        ps2.x=p[0]-ps5.x;ps2.y=p[1]-ps5.y;ps2.z=p[2]-ps5.z;
        ln=njSqrt(ps2.x*ps2.x+ps2.z*ps2.z);
        if(ln<HAIR_SHOULDER_CLEARANCE){
            ps2.y=0;
            njUnitVector(&ps2);
            p[0]=ps5.x+HAIR_SHOULDER_CLEARANCE*ps2.x;
            p[2]=ps5.z+HAIR_SHOULDER_CLEARANCE*ps2.z;
        }

        /* objitm.c:2336-2351: derselbe Hoechstabstand erneut, weil der
         * Schulterabstand den Punkt verschoben haben kann.  ps1 behaelt danach
         * den Restvektor, den die Winkelbildung benutzt. */
        ps1.x=p[0]-px;ps1.y=p[1]-py;ps1.z=p[2]-pz;
        if(njScalor(&ps1)>HAIR_SEGMENT_REACH){
            njUnitVector(&ps1);
            ps1.x*=HAIR_SEGMENT_REACH;ps1.y*=HAIR_SEGMENT_REACH;ps1.z*=HAIR_SEGMENT_REACH;
            p[0]=px+ps1.x;p[1]=py+ps1.y;p[2]=pz+ps1.z;
        }

        /* objitm.c:2353-2368: rx/ry nehmen die von den Vorgaengersegmenten
         * schon aufgebrachte Drehung wieder heraus, damit die Winkel lokal
         * zum jeweiligen Elternknoten entstehen. */
        njUnitMatrix(&m);
        njRotateX(&m,rx);
        njRotateY(&m,ry);
        njCalcPoint(&m,&ps1,&ps3);

        ay=(int16_t)(int32_t)(HAIR_ANGLE_FROM_ATAN*atan2f(ps3.x,HAIR_YAW_BIAS+ps3.z));
        ry=(int16_t)(ry-ay);
        angles[i][1]=ay;

        ax=(int16_t)(int32_t)(HAIR_ANGLE_PER_DEGREE*(HAIR_PITCH_GAIN*-ps3.y));
        rx=(int16_t)(rx-ax);
        angles[i][0]=ax;

        /* objitm.c:2370-2376: das naechste Segment haengt an diesem Punkt. */
        px=p[0];py=p[1];pz=p[2];
        s->pv[i][0]=px;s->pv[i][1]=py;s->pv[i][2]=pz;
        s->angles[i][0]=angles[i][0];
        s->angles[i][1]=angles[i][1];
    }

    /* objitm.c:2379 */
    s->ct0=(s->ct0+1)&(RECVX_HAIR_HISTORY-1);
    return 1;
}
