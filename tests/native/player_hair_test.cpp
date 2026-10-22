// Haarkette des angehaengten Spielerobjekts obwp[2] (Auftrag B04).
//
// Geprueft werden Initialisierung, Fortschreibung, Knoten-/Winkelwirkung,
// Reset und beide Varianten des Originalverteilers bhJumpObject2, jeweils
// gegen die Semantik aus objitm.c:2122-2383 und player.c:7207-7298.
#include "player_hair.h"
#include "ninja_matrix.h"
#include "ninja_math.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}

namespace {
// objitm.c:2299/2340 begrenzt jedes Segment auf 0.5 zum Vorgaenger; die
// Pruefung laesst die Rundung der Einheitsvektoren zu.
constexpr float HAIR_REACH_TOLERANCE=0.5f+1e-3f;
// Kopfknoten owP[5] wie der Host ihn liefert: Weltmatrix aus Gier und Lage.
void head_matrix(float out[16],float x,float y,float z,int yaw){
    NJS_MATRIX m;njUnitMatrix(&m);njTranslate(&m,x,y,z);njRotateY(&m,yaw);
    std::copy_n(static_cast<const float*>(m),16,out);
}
// Wurzel der Kette: die Objektmatrix von bhCalcModel (MdlPut.c:244-246).
NJS_POINT3 root_of(const RECVX_HAIR_INPUT& in){
    NJS_MATRIX head;std::copy_n(in.head,16,head);
    NJS_POINT3 local{in.offset[0],in.offset[1],in.offset[2]},out;
    njCalcPoint(&head,&local,&out);return out;
}
float distance(const float a[3],const NJS_POINT3& b){
    const float dx=a[0]-b.x,dy=a[1]-b.y,dz=a[2]-b.z;return std::sqrt(dx*dx+dy*dy+dz*dz);
}
float planar(const float a[3],const NJS_POINT3& b){
    const float dx=a[0]-b.x,dz=a[2]-b.z;return std::sqrt(dx*dx+dz*dz);
}
NJS_POINT3 head_point(const RECVX_HAIR_INPUT& in,float x,float y,float z){
    NJS_MATRIX head;std::copy_n(in.head,16,head);
    NJS_POINT3 local{x,y,z},out;njCalcPoint(&head,&local,&out);return out;
}
}

int main(){try{
    _Make_SinTable();

    // --- Variantenwahl: bhSetPlayer/bhResetPlayer binden obwp[2] nur fuer
    // ply_id 0 und 3 (player.c:756-785).  bhJumpObject2 (objitm.c:125-140)
    // fuehrt id 1211 auf bhObjClpn, id 1212 dagegen auf bhObjWssg mit leerem
    // Koerper - Spieler 3 hat im Original keine Haarphysik.
    {RECVX_HAIR_STATE s{};
     check(recvx_hair_begin(&s,0)==RECVX_HAIR_VARIANT_CLPN,"ply_id 0 must select bhObjClpn");
     check(recvx_hair_begin(&s,3)==RECVX_HAIR_VARIANT_WSSG,"ply_id 3 must select bhObjWssg");
     check(recvx_hair_begin(&s,1)==RECVX_HAIR_VARIANT_NONE,"ply_id 1 has no lkmtab entry");
     check(recvx_hair_begin(&s,2)==RECVX_HAIR_VARIANT_NONE,"ply_id 2 has no lkmtab entry");}

    // --- Anfangszustand: bhSetObject nullt das Werk, flg kommt aus dem
    // lkmtab-Eintrag (player.c:24, 129 = 0x81).
    {RECVX_HAIR_STATE s{};recvx_hair_begin(&s,0);
     check(s.flg==0x81u,"lkmtab flg 0x81 (linked + moving) not set");
     check(s.mode0==0&&s.ct0==0&&s.spd==0,"object work not cleared");
     check(s.xn==0&&s.yn==0&&s.zn==0,"filtered velocity not cleared");}

    // --- Die leere Variante erzeugt nie Winkel, auch bei Bewegung.
    {RECVX_HAIR_STATE s{};const auto variant=recvx_hair_begin(&s,3);
     RECVX_HAIR_INPUT in{};head_matrix(in.head,0,10,0,0);in.offset[1]=1.5869f;in.reset=1;
     std::int32_t out[RECVX_HAIR_SEGMENTS][2];
     for(int f=0;f<8;++f){head_matrix(in.head,float(f),10,0,0);
        check(recvx_hair_tick(&s,variant,&in,out)==0,"bhObjWssg produced a chain frame");}
     check(s.mode0==0,"bhObjWssg advanced the chain state");}

    // --- mode0 0: das erste Bild initialisiert nur (objitm.c:2139-2178) und
    // liefert noch keine Winkel; danach laeuft mode0 1.
    RECVX_HAIR_STATE s{};const auto variant=recvx_hair_begin(&s,0);
    RECVX_HAIR_INPUT in{};
    in.offset[0]=0;in.offset[1]=1.5869f;in.offset[2]=.7747f; // player.c:763-765
    head_matrix(in.head,0,10,0,0);
    std::int32_t out[RECVX_HAIR_SEGMENTS][2];
    check(recvx_hair_tick(&s,variant,&in,out)==0,"the initialising frame must not produce angles");
    check(s.mode0==1&&(s.flg&0x1000u)&&(s.flg&0x100000u),"case 0 did not open the buffers");
    for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)check(!out[i][0]&&!out[i][1],"case 0 returned angles");

    // --- Stillstand: die Kette haelt ihre Abstaende und laeuft in einen
    // festen Zustand.  Das Original setzt pp->flg2 0x2 beim Raumwechsel,
    // hier genau einmal fuer das erste Laufbild.
    in.reset=1;
    check(recvx_hair_tick(&s,variant,&in,out)==1,"the first running frame produced nothing");
    in.reset=0;
    for(int f=0;f<120;++f)check(recvx_hair_tick(&s,variant,&in,out)==1,"a resting frame produced nothing");
    {const auto root=root_of(in);
     const auto top=head_point(in,0,1.0f,0);
     const auto shoulder_raw=head_point(in,0,1.8f,-.3f);
     NJS_POINT3 shoulder{shoulder_raw.x,top.y,shoulder_raw.z};
     // objitm.c:2299/2340: jedes Segment bleibt 0.5 vom Vorgaenger entfernt.
     // Im Ruhezustand haengt die Kette gestreckt, der Abstand ist also genau
     // der Hoechstabstand.
     NJS_POINT3 previous=root;
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        const float reach=distance(s.pv[i],previous);
        check(reach<=HAIR_REACH_TOLERANCE,"segment exceeded its 0.5 reach");
        check(std::abs(reach-0.5f)<1e-3f,"resting chain is not hanging stretched");
        previous=NJS_POINT3{s.pv[i][0],s.pv[i][1],s.pv[i][2]};}
     // objitm.c:2281/2326: die beiden Mindestabstaende wirken auf den
     // Zielpunkt g3p.  Der danach folgende 0.5-Riegel (objitm.c:2299/2340)
     // darf den gefilterten Punkt wieder geringfuegig hineinziehen, deshalb
     // wird die Grenze an der Groesse geprueft, auf die sie im Original
     // angewandt wird.
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(planar(s.goal[i],top)>=1.0f-1e-3f,"chain goal entered the 1.0 head clearance");
        check(planar(s.goal[i],shoulder)>=1.3f-1e-3f,"chain goal entered the 1.3 shoulder clearance");}
     // Im Ruhezustand haengt die Kette genau auf den beiden Grenzen.
     check(std::abs(planar(s.goal[0],top)-1.0f)<1e-3f,"resting goal is not on the head clearance");
     check(std::abs(planar(s.goal[0],shoulder)-1.3f)<1e-3f,"resting goal is not on the shoulder clearance");}

    // --- Ruhezustand ist stabil: zwei weitere Bilder aendern die Pose nicht
    // mehr messbar.
    {float before[RECVX_HAIR_SEGMENTS][3];std::memcpy(before,s.pv,sizeof before);
     std::int32_t angles_before[RECVX_HAIR_SEGMENTS][2];std::memcpy(angles_before,s.angles,sizeof angles_before);
     recvx_hair_tick(&s,variant,&in,out);recvx_hair_tick(&s,variant,&in,out);
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(distance(before[i],NJS_POINT3{s.pv[i][0],s.pv[i][1],s.pv[i][2]})<1e-3f,"resting chain kept drifting");
        check(std::abs(angles_before[i][0]-s.angles[i][0])<=1&&std::abs(angles_before[i][1]-s.angles[i][1])<=1,
              "resting chain angles kept changing");}}

    // --- Vorwaerts: die Wurzelverschiebung erzeugt eine Geschwindigkeit, die
    // der Kette entgegen laeuft (objitm.c:2210-2216, negatives Vorzeichen).
    {RECVX_HAIR_STATE f{};const auto v=recvx_hair_begin(&f,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&f,v,&m,out);m.reset=1;
     float z=0;for(int i=0;i<40;++i){z-=1.0f;head_matrix(m.head,0,10,z,0);
        check(recvx_hair_tick(&f,v,&m,out)==1,"a forward frame produced nothing");m.reset=0;}
     // Die Wurzel laeuft nach -z, hair->spz ist begrenzt negativ, der Puffer
     // haelt also das gegengerichtete +z.
     check(f.hair.spz<=-1.0f+1e-3f,"forward motion did not saturate hair->spz");
     check(f.zn>0,"the filtered velocity did not oppose the forward motion");
     const auto root=root_of(m);
     // Jedes Segment liegt weiter hinten als sein Vorgaenger: die Kette
     // streckt sich entgegen der Laufrichtung.
     check(f.pv[0][2]>root.z,"the chain did not trail behind the forward motion");
     for(unsigned i=1;i<RECVX_HAIR_SEGMENTS;++i)
        check(f.pv[i][2]>f.pv[i-1][2],"the chain did not stretch out segment by segment");
     // Die Abstandsgrenzen gelten auch in Bewegung.
     NJS_POINT3 previous=root;
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(distance(f.pv[i],previous)<=HAIR_REACH_TOLERANCE,"moving segment exceeded its 0.5 reach");
        previous=NJS_POINT3{f.pv[i][0],f.pv[i][1],f.pv[i][2]};}}

    // --- Rueckwaerts: dieselbe Mechanik mit umgekehrtem Vorzeichen.
    {RECVX_HAIR_STATE b{};const auto v=recvx_hair_begin(&b,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&b,v,&m,out);m.reset=1;
     float z=0;for(int i=0;i<40;++i){z+=1.0f;head_matrix(m.head,0,10,z,0);
        check(recvx_hair_tick(&b,v,&m,out)==1,"a backward frame produced nothing");m.reset=0;}
     check(b.hair.spz>=1.0f-1e-3f,"backward motion did not saturate hair->spz");
     check(b.zn<0,"the filtered velocity did not oppose the backward motion");
     // Das Haar haengt hinter dem Kopf (loz +0.7747).  Rueckwaerts laufen
     // treibt es gegen den Kopf, und genau dafuer gibt es die beiden
     // Mindestabstaende: die Kette bleibt auf der Grenze stehen, statt durch
     // den Kopf zu wandern.  Gemessen liegt sie auf beiden Grenzen zugleich.
     {const auto top=head_point(m,0,1.0f,0);
      const auto shoulder_raw=head_point(m,0,1.8f,-.3f);
      NJS_POINT3 shoulder{shoulder_raw.x,top.y,shoulder_raw.z};
      for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
         check(planar(b.pv[i],top)>=1.0f-1e-3f,"backward motion pushed the chain into the head");
         check(planar(b.pv[i],shoulder)>=1.3f-1e-3f,"backward motion pushed the chain into the shoulder");}
      check(std::abs(planar(b.pv[0],top)-1.0f)<1e-3f,"the backward chain does not rest on the head clearance");}}

    // --- Drehen: die Giersumme aus plp->ay und den Knotenwinkeln bestimmt die
    // Richtung, in die der Verlauf gedreht wird (player.c:7219-7241).
    {RECVX_HAIR_STATE r{};const auto v=recvx_hair_begin(&r,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&r,v,&m,out);m.reset=1;
     for(int i=0;i<40;++i){const int yaw=i*819;m.ay=yaw;head_matrix(m.head,0,10,0,yaw);
        check(recvx_hair_tick(&r,v,&m,out)==1,"a turning frame produced nothing");m.reset=0;}
     check(r.hair.ay==std::int16_t(39*819),"the yaw sum did not follow plp->ay");
     // Die Knotenwinkel des Spielers gehen in dieselbe Summe ein.
     m.angles[0][1]=1000;m.angles[5][1]=-300;
     recvx_hair_tick(&r,v,&m,out);
     check(r.hair.ay==std::int16_t(39*819+1000-300),"node angles are missing from the yaw sum");
     // Die Abstaende halten auch beim Drehen.
     const auto root=root_of(m);NJS_POINT3 previous=root;
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(distance(r.pv[i],previous)<=HAIR_REACH_TOLERANCE,"turning segment exceeded its 0.5 reach");
        previous=NJS_POINT3{r.pv[i][0],r.pv[i][1],r.pv[i][2]};}}

    // --- plp->ax/az gehen in dieselbe Summe ein wie ay (player.c:7219-7221).
    // Das Original schreibt sie nie (nur map.c:1074, player.c:1375/1377,
    // bhControlPlayerHead und bhCalcHair lesen sie), die Kette muss sie aber
    // vom Spielerwerk uebernehmen, sobald ein Besitzer sie setzt.
    {RECVX_HAIR_STATE p{};const auto v=recvx_hair_begin(&p,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&p,v,&m,out);
     m.ax=2000;m.az=-1500;m.ay=700;m.angles[2][0]=300;m.angles[4][2]=-100;
     recvx_hair_tick(&p,v,&m,out);
     check(p.hair.ax==std::int16_t(2000+300),"plp->ax is missing from the pitch sum");
     check(p.hair.az==std::int16_t(-1500-100),"plp->az is missing from the roll sum");
     check(p.hair.ay==std::int16_t(700),"plp->ay is missing from the yaw sum");
     // Die Umkehrdrehung njRotateY(-ay)/njRotateX(-ax)/njRotateZ(-az) benutzt
     // alle drei Winkel.  Gemessen an einer Bewegung, die die +-1-Begrenzung
     // nicht erreicht - gesaettigte Werte wuerden den Unterschied verdecken.
     auto probe=[&](int ax,int az){
        RECVX_HAIR_STATE q{};recvx_hair_begin(&q,0);
        RECVX_HAIR_INPUT n{};n.offset[1]=1.5869f;n.offset[2]=.7747f;n.ax=ax;n.az=az;
        std::int32_t o2[RECVX_HAIR_SEGMENTS][2];
        head_matrix(n.head,0,10,0,0);recvx_hair_tick(&q,v,&n,o2);
        head_matrix(n.head,0,10,-0.4f,0);recvx_hair_tick(&q,v,&n,o2);
        return q.hair;};
     const auto flat=probe(0,0),pitched=probe(8192,0);
     check(std::abs(flat.spz)<1.0f-1e-3f,"the probe motion must stay below the clamp");
     check(std::abs(pitched.spz-flat.spz)>1e-3f,
           "plp->ax does not reach the inverse rotation of bhCalcHair");
     // az dreht x/y, eine reine z-Bewegung bleibt davon unberuehrt - das ist
     // njRotateZ, keine fehlende Bindung.  Eine x-Bewegung zeigt die Wirkung.
     auto probe_x=[&](int az){
        RECVX_HAIR_STATE q{};recvx_hair_begin(&q,0);
        RECVX_HAIR_INPUT n{};n.offset[1]=1.5869f;n.offset[2]=.7747f;n.az=az;
        std::int32_t o2[RECVX_HAIR_SEGMENTS][2];
        head_matrix(n.head,0,10,0,0);recvx_hair_tick(&q,v,&n,o2);
        head_matrix(n.head,-0.4f,10,0,0);recvx_hair_tick(&q,v,&n,o2);
        return q.hair;};
     const auto flat_x=probe_x(0),rolled=probe_x(8192);
     check(std::abs(rolled.spx-flat_x.spx)>1e-3f||std::abs(rolled.spz-flat_x.spz)>1e-3f,
           "plp->az does not reach the inverse rotation of bhCalcHair");}

    // --- Reset: flg2 0x2 loescht Verlauf und HAIR_WORK (objitm.c:2184-2204).
    // Danach steht die Kette wieder wie nach dem ersten Laufbild.
    {RECVX_HAIR_STATE a{};const auto v=recvx_hair_begin(&a,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&a,v,&m,out);m.reset=1;
     float z=0;for(int i=0;i<30;++i){z-=1.0f;head_matrix(m.head,0,10,z,0);recvx_hair_tick(&a,v,&m,out);m.reset=0;}
     check(a.zn!=0||a.spd!=0,"the moving chain carried no history");
     // Raumwechsel: neue Lage und Reset im selben Bild.
     head_matrix(m.head,100,4,-50,0);m.reset=1;
     recvx_hair_tick(&a,v,&m,out);
     const auto root=root_of(m);
     // Die neue Wurzel ist uebernommen und die alte ist weg.
     check(std::abs(a.hair.ox-root.x)<1e-3f&&std::abs(a.hair.oz-root.z)<1e-3f,"the reset frame kept the old root");
     // Originaleigenart: objitm.c:2189 nullt HAIR_WORK, und erst danach laeuft
     // bhCalcHair (objitm.c:2206).  Der Vorgaenger der Wurzel ist in diesem
     // einen Bild also der Ursprung, nicht die vorige Lage - die
     // Geschwindigkeit dieses Bildes laeuft folglich in die Begrenzung.  Das
     // ist bewusst so uebernommen; wichtig ist, dass sie begrenzt bleibt.
     check(std::abs(a.hair.oxb)<1e-6f&&std::abs(a.hair.ozb)<1e-6f,"the reset did not clear the previous root");
     check(std::abs(a.hair.spx)<=1.0f+1e-4f&&std::abs(a.hair.spy)<=1.0f+1e-4f&&std::abs(a.hair.spz)<=1.0f+1e-4f,
           "the reset frame produced an unbounded root velocity");
     // Trotz dieser Eigenart haengt die Kette an der neuen Raumlage, nicht
     // mehr an der alten: jedes Segment liegt im Greifbereich der Wurzel.
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)
        check(distance(a.pv[i],root)<=HAIR_REACH_TOLERANCE*(i+1),
              "the chain stayed at the old room position after the reset");
     // Der Verlaufspuffer ist wirklich geloescht.
     int nonzero=0;for(unsigned i=0;i<RECVX_HAIR_HISTORY;++i)
        if(a.history[i][0]||a.history[i][1]||a.history[i][2])++nonzero;
     check(nonzero<=1,"the reset left old history entries behind");}

    // --- Ringpuffer: ct0 laeuft mit Maske 0x7F um (objitm.c:2379) und die
    // Segmente lesen je drei Bilder weiter zurueck (objitm.c:2234).
    {RECVX_HAIR_STATE w{};const auto v=recvx_hair_begin(&w,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;head_matrix(m.head,0,10,0,0);
     recvx_hair_tick(&w,v,&m,out);
     for(int i=0;i<RECVX_HAIR_HISTORY;++i)recvx_hair_tick(&w,v,&m,out);
     check(w.ct0==0,"the 128-entry history did not wrap at 0x7F");
     for(int i=0;i<5;++i)recvx_hair_tick(&w,v,&m,out);
     check(w.ct0==5,"the write cursor did not continue after the wrap");}

    // --- Der Fallanteil ist begrenzt (objitm.c:2255-2262).
    {RECVX_HAIR_STATE d{};const auto v=recvx_hair_begin(&d,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;head_matrix(m.head,0,10,0,0);
     recvx_hair_tick(&d,v,&m,out);
     for(int i=0;i<200;++i)recvx_hair_tick(&d,v,&m,out);
     check(d.spd>=-0.5f-1e-4f&&d.spd<=0.5f+1e-4f,"the fall term left its +-0.5 limit");}

    // --- Die Geschwindigkeit ist auf +-1 begrenzt (player.c:7269-7297), auch
    // bei einem Sprung ueber den halben Raum.
    {RECVX_HAIR_STATE j{};const auto v=recvx_hair_begin(&j,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;head_matrix(m.head,0,10,0,0);
     recvx_hair_tick(&j,v,&m,out);
     head_matrix(m.head,500,300,-900,0);recvx_hair_tick(&j,v,&m,out);
     check(std::abs(j.hair.spx)<=1.0f+1e-4f,"hair->spx left its +-1 limit");
     check(std::abs(j.hair.spy)<=1.0f+1e-4f,"hair->spy left its +-1 limit");
     check(std::abs(j.hair.spz)<=1.0f+1e-4f,"hair->spz left its +-1 limit");}

    // --- Die Winkel sind die gemeldete Wirkung auf die Knoten: sie liegen im
    // Bereich eines short (objitm.c:2127) und werden auch im Zustand gefuehrt.
    {RECVX_HAIR_STATE n{};const auto v=recvx_hair_begin(&n,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&n,v,&m,out);
     float z=0;for(int i=0;i<30;++i){z-=1.0f;head_matrix(m.head,0,10,z,0);recvx_hair_tick(&n,v,&m,out);}
     bool any=false;
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(out[i][0]>=-32768&&out[i][0]<=32767,"pitch angle outside the original short range");
        check(out[i][1]>=-32768&&out[i][1]<=32767,"yaw angle outside the original short range");
        check(n.angles[i][0]==out[i][0]&&n.angles[i][1]==out[i][1],"state angles disagree with the returned ones");
        if(out[i][0]||out[i][1])any=true;}
     check(any,"the moving chain produced no node angles at all");}

    // --- op->pv und die Knotenwinkel beschreiben dieselbe Kette: pv[i] ist
    // der Punkt, an dem Segment i endet (objitm.c:2370-2376), und genau dieser
    // Punkt ist der Ausgangspunkt der Winkelbildung des naechsten Segments.
    // pv hat im Original keinen aktiven Leser (bhDrawLinEffect bezieht seine
    // Liste aus sys->ef_lin, das nie gefuellt wird); geprueft wird deshalb die
    // Konsistenz mit dem, was der Zeichenpass benutzt.
    {RECVX_HAIR_STATE c{};const auto v=recvx_hair_begin(&c,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;m.offset[2]=.7747f;
     head_matrix(m.head,0,10,0,0);recvx_hair_tick(&c,v,&m,out);
     float z=0;for(int i=0;i<40;++i){z-=1.0f;head_matrix(m.head,0,10,z,0);recvx_hair_tick(&c,v,&m,out);}
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i)
        for(unsigned a=0;a<3;++a)
           check(std::abs(c.pv[i][a]-c.point[i][a])<1e-6f,"pv does not hold the filtered chain point");
     // Jedes Segment haengt am Vorgaenger, nicht an der Wurzel.
     const auto root=root_of(m);NJS_POINT3 previous=root;
     for(unsigned i=0;i<RECVX_HAIR_SEGMENTS;++i){
        check(distance(c.pv[i],previous)<=HAIR_REACH_TOLERANCE,"pv chain broke its 0.5 reach");
        previous=NJS_POINT3{c.pv[i][0],c.pv[i][1],c.pv[i][2]};}}

    // --- Der Haltezustand mode0 2 stoppt den Task (objitm.c:2135).
    {RECVX_HAIR_STATE h{};const auto v=recvx_hair_begin(&h,0);
     RECVX_HAIR_INPUT m{};m.offset[1]=1.5869f;head_matrix(m.head,0,10,0,0);
     recvx_hair_tick(&h,v,&m,out);recvx_hair_tick(&h,v,&m,out);
     h.mode0=2;check(recvx_hair_tick(&h,v,&m,out)==0,"mode0 2 still ran the chain");
     // Ein nicht angehaengtes Objekt laeuft ebenfalls nicht.
     h.mode0=1;h.flg&=~0x80u;check(recvx_hair_tick(&h,v,&m,out)==0,"an unlinked object still ran the chain");}

    std::cout<<"player hair chain ok\n";return 0;
}catch(const std::exception& e){std::cerr<<"player hair: "<<e.what()<<'\n';return 1;}}
