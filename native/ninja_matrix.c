// Host translation of the affine matrix/vector operations in ps2_NaMatrix.c.
// Column-major storage and rotation signs follow the original VU operations.
#include "ninja_matrix.h"
#include "widescreen.h"
#include <string.h>
#include <math.h>
#include <float.h>
static NJS_MATRIX default_matrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static NJS_MATRIX* stack_top=&default_matrix;
static Sint32 stack_count=0, stack_capacity=1;
NJS_MATRIX* pNaMatMatrixStuckPtr=&default_matrix;
NJS_MATRIX NaViwViewMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static Float* resolve(NJS_MATRIX* m) { return m ? *m : *pNaMatMatrixStuckPtr; }
void njInitMatrix(NJS_MATRIX* m, Sint32 n, Int flag) {
    (void)flag;
    stack_top=m && n>0 ? m : &default_matrix;
    stack_capacity=m && n>0 ? n : 1;
    stack_count=0;
    pNaMatMatrixStuckPtr=stack_top;
    njUnitMatrix(NULL);
}
void njUnitMatrix(NJS_MATRIX* m) {
    Float* p=resolve(m);
    memset(p,0,sizeof(NJS_MATRIX));p[0]=p[5]=p[10]=p[15]=1;
}
void njSetMatrix(NJS_MATRIX* dest,NJS_MATRIX* source) { memmove(resolve(dest),resolve(source),sizeof(NJS_MATRIX)); }
void njGetMatrix(NJS_MATRIX* dest) { njSetMatrix(dest,NULL); }
void njCopyMatrix(NJS_MATRIX* dest,NJS_MATRIX* source) { njSetMatrix(dest,source); }
void njClearMatrix(void) { stack_count=0;pNaMatMatrixStuckPtr=stack_top;njSetMatrix(NULL,&NaViwViewMatrix); }
Bool njPushMatrix(NJS_MATRIX* source) {
    if (stack_count+1>=stack_capacity) return 0;
    memmove(stack_top[stack_count+1],resolve(source),sizeof(NJS_MATRIX));
    ++stack_count;pNaMatMatrixStuckPtr=&stack_top[stack_count];return 1;
}
Bool njPopMatrix(Uint32 count) {
    if (count>(Uint32)stack_count) return 0;
    stack_count-=(Sint32)count;pNaMatMatrixStuckPtr=&stack_top[stack_count];return 1;
}
Bool njPushMatrixEx(void) { return njPushMatrix(NULL); }
Bool njPopMatrixEx(void) { return njPopMatrix(1); }
void njMultiMatrix(NJS_MATRIX* dest,NJS_MATRIX* source) {
    Float* a=resolve(dest);Float* b=resolve(source);NJS_MATRIX result;int c,r,k;
    for(c=0;c<4;++c) for(r=0;r<4;++r) {
        result[c*4+r]=0;
        for(k=0;k<4;++k) result[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    memcpy(a,result,sizeof(result));
}
void njTranslate(NJS_MATRIX* m,Float x,Float y,Float z) {
    Float* p=resolve(m);int r;
    for(r=0;r<3;++r) p[12+r]+=(p[r]*x+p[4+r]*y)+p[8+r]*z;
}
void njTranslateV(NJS_MATRIX* m,NJS_VECTOR* v) { njTranslate(m,v->x,v->y,v->z); }
void njScale(NJS_MATRIX* m,Float x,Float y,Float z) {
    Float* p=resolve(m);int i;
    for(i=0;i<3;++i){p[i]*=x;p[4+i]*=y;p[8+i]*=z;}
}
void njScaleV(NJS_MATRIX* m,NJS_VECTOR* v) { njScale(m,v->x,v->y,v->z); }
static void rotate(NJS_MATRIX* m,int a,int b,Angle angle,Bool inverse) {
    Float* p=resolve(m);Float s,c;int i;njSinCos(angle,&s,&c);if(inverse)s=-s;
    for(i=0;i<3;++i) {
        Float x=p[a*4+i],y=p[b*4+i];p[a*4+i]=x*c+y*s;p[b*4+i]=-x*s+y*c;
    }
}
void njRotateX(NJS_MATRIX* m,Angle angle) { rotate(m,1,2,angle,0); }
void njRotateY(NJS_MATRIX* m,Angle angle) { rotate(m,0,2,angle,1); }
void njRotateZ(NJS_MATRIX* m,Angle angle) { rotate(m,0,1,angle,0); }
void njRotateXYZ(NJS_MATRIX* m,Angle x,Angle y,Angle z) { njRotateZ(m,z);njRotateY(m,y);njRotateX(m,x); }
/* ps2_NaMatrix.c:737 — Drehung um eine freie Achse. Die Originalroutine
   rechnet in der VU0, der Mechanismus ist aber reine Quaternionenmathematik
   und wird hier eigenstaendig nativ gefuehrt; es wird keine VU nachgebildet.
   Aus der Originalfolge belegt und hier erhalten:
     :745  ang &= 0xFFFF, DANN ang /= 2 — der HALBE Winkel, auf dem gemaskten
           positiven Wert. Ein negativer Winkel wird also erst in seine
           16-Bit-Darstellung gebracht und dann halbiert.
     :762  vrsqrt auf x*x+y*y+z*z: die Achse WIRD normiert, der Aufrufer muss
           keinen Einheitsvektor liefern. Eine Achse der Laenge null laeuft
           wie im Original ungeschuetzt durch njInvertSqrt (ninja_math.c:27).
     :774  der Vektorteil ist Achse*sin(ang/2), der Skalarteil cos(ang/2).
     :787ff die drei Spalten der Drehmatrix in genau dieser Form, also mit
           c*c statt der Vereinfachung 1-2y*y-2z*z: beide sind nur fuer einen
           exakten Einheitsquaternion gleich.
     :813  die Verkettung ist eine Rechtsmultiplikation m = m * R, wie bei
           njRotateX/Y/Z. Sie schreibt mit vmul/vadd.xyz NUR die Komponenten
           x,y,z der drei Spalten: die vierte Zeile (p[3], p[7], p[11]) und
           die Translationsspalte bleiben unberuehrt. Das ist hier genauso. */
void njRotate(NJS_MATRIX* m,NJS_VECTOR* v,Angle ang) {
    Float* p=resolve(m);Float s,c,q,x,y,z,col[3][3];int i,r;
    njSinCos((ang&0xFFFF)/2,&s,&c);
    q=njInvertSqrt(v->x*v->x+v->y*v->y+v->z*v->z)*s;
    x=v->x*q;y=v->y*q;z=v->z*q;
    col[0][0]=c*c+x*x-y*y-z*z; col[0][1]=2*(x*y+c*z);     col[0][2]=2*(x*z-c*y);
    col[1][0]=2*(x*y-c*z);     col[1][1]=c*c-x*x+y*y-z*z; col[1][2]=2*(y*z+c*x);
    col[2][0]=2*(x*z+c*y);     col[2][1]=2*(y*z-c*x);     col[2][2]=c*c-x*x-y*y+z*z;
    for (i=0;i<3;++i) {
        Float out[3];
        for (r=0;r<3;++r)
            out[r]=(p[r]*col[i][0]+p[4+r]*col[i][1])+p[8+r]*col[i][2];
        col[i][0]=out[0];col[i][1]=out[1];col[i][2]=out[2];
    }
    for (i=0;i<3;++i) for (r=0;r<3;++r) p[i*4+r]=col[i][r];
}
void njRotXYZ(NJS_MATRIX* m,int x,int y,int z) { njRotateX(m,x);njRotateY(m,y);njRotateZ(m,z); }
void njCalcPoint4(NJS_MATRIX* m,NJS_POINT4* source,NJS_POINT4* dest) {
    Float* p=resolve(m);NJS_POINT4 a=*source,b;
    b.x=((p[0]*a.x+p[4]*a.y)+p[8]*a.z)+p[12]*a.w;
    b.y=((p[1]*a.x+p[5]*a.y)+p[9]*a.z)+p[13]*a.w;
    b.z=((p[2]*a.x+p[6]*a.y)+p[10]*a.z)+p[14]*a.w;
    b.w=((p[3]*a.x+p[7]*a.y)+p[11]*a.z)+p[15]*a.w;*dest=b;
}
void njCalcPoint(NJS_MATRIX* m,NJS_POINT3* source,NJS_POINT3* dest) {
    NJS_POINT4 a={source->x,source->y,source->z,1},b;
    njCalcPoint4(m,&a,&b);dest->x=b.x;dest->y=b.y;dest->z=b.z;
}
void njCalcPoints(NJS_MATRIX* m,NJS_POINT3* source,NJS_POINT3* dest,Int count) {
    int i;for(i=0;i<count;++i)njCalcPoint(m,&source[i],&dest[i]);
}
void njCalcVector(NJS_MATRIX* m,NJS_VECTOR* source,NJS_VECTOR* dest) {
    NJS_POINT4 a={source->x,source->y,source->z,0},b;
    njCalcPoint4(m,&a,&b);dest->x=b.x;dest->y=b.y;dest->z=b.z;
}
void njGetTranslation(NJS_MATRIX* m,NJS_POINT3* dest) { Float* p=resolve(m);dest->x=p[12];dest->y=p[13];dest->z=p[14]; }
void njUnitTransPortion(NJS_MATRIX* m) { Float* p=resolve(m);p[12]=p[13]=p[14]=0;p[15]=1; }
void njUnitRotPortion(NJS_MATRIX* m) {
    Float* p=resolve(m);int i;for(i=0;i<12;++i)p[i]=0;p[0]=p[5]=p[10]=1;
}
void njTransposeMatrix(NJS_MATRIX* m) {
    Float* p=resolve(m);int r,c;for(r=0;r<4;++r)for(c=r+1;c<4;++c){Float a=p[r*4+c];p[r*4+c]=p[c*4+r];p[c*4+r]=a;}
}
Bool njInvertMatrix(NJS_MATRIX* m) {
    Float* p=resolve(m);double a[4][8];int r,c,i;
    for(r=0;r<4;++r)for(c=0;c<8;++c)a[r][c]=c<4?p[c*4+r]:(c-4==r?1:0);
    for(c=0;c<4;++c) {
        int pivot=c;double divisor;
        for(r=c+1;r<4;++r)if(fabs(a[r][c])>fabs(a[pivot][c]))pivot=r;
        if(!isfinite(a[pivot][c])||fabs(a[pivot][c])<DBL_MIN)return 0;
        if(pivot!=c)for(i=0;i<8;++i){double t=a[c][i];a[c][i]=a[pivot][i];a[pivot][i]=t;}
        divisor=a[c][c];for(i=0;i<8;++i)a[c][i]/=divisor;
        for(r=0;r<4;++r)if(r!=c){double factor=a[r][c];for(i=0;i<8;++i)a[r][i]-=factor*a[c][i];}
    }
    for(r=0;r<4;++r)for(c=0;c<4;++c)if(!isfinite(a[r][c+4])||fabs(a[r][c+4])>FLT_MAX)return 0;
    for(r=0;r<4;++r)for(c=0;c<4;++c)p[c*4+r]=(Float)a[r][c+4];return 1;
}
void njAddVector(NJS_VECTOR* d,NJS_VECTOR* s){d->x+=s->x;d->y+=s->y;d->z+=s->z;}
void njSubVector(NJS_VECTOR* d,NJS_VECTOR* s){d->x-=s->x;d->y-=s->y;d->z-=s->z;}
Float njScalor2(NJS_VECTOR* v){return (v->x*v->x+v->y*v->y)+v->z*v->z;}
Float njScalor(NJS_VECTOR* v){return njSqrt(njScalor2(v));}
Float njUnitVector(NJS_VECTOR* v){Float n=njScalor(v);if(n>0){v->x/=n;v->y/=n;v->z/=n;}return n;}
Float njInnerProduct(NJS_VECTOR* a,NJS_VECTOR* b){return(a->x*b->x+a->y*b->y)+a->z*b->z;}
Float njOuterProduct(NJS_VECTOR* a,NJS_VECTOR* b,NJS_VECTOR* dest){
    NJS_VECTOR result={a->y*b->z-a->z*b->y,a->z*b->x-a->x*b->z,a->x*b->y-a->y*b->x};*dest=result;return njScalor(&result);
}
// ps2_NaColi.c:301 njGetPlaneNormal — Kreuzprodukt ueber die ersten DREI
// Punkte des Feldes, unnormiert; der Aufrufer normiert selbst.
void njGetPlaneNormal(NJS_POINT3* p,NJS_VECTOR* v){
    NJS_POINT3* p2=&p[1];NJS_POINT3* p3=&p[2];
    v->x=((p2->y-p->y)*(p3->z-p2->z))-((p2->z-p->z)*(p3->y-p2->y));
    v->y=((p2->z-p->z)*(p3->x-p2->x))-((p2->x-p->x)*(p3->z-p2->z));
    v->z=((p2->x-p->x)*(p3->y-p2->y))-((p2->y-p->y)*(p3->x-p2->x));
}
// ps2_NaColi.c:314 njGetPlaneNormal2 — dieselbe Normale aus drei Punkten.
// Die zweite Kante ist p2-p1 (nicht p2-p0); das Vorzeichen von v haengt daran.
void njGetPlaneNormal2(NJS_POINT3* p0,NJS_POINT3* p1,NJS_POINT3* p2,NJS_VECTOR* v){
    v->x=((p1->y-p0->y)*(p2->z-p1->z))-((p1->z-p0->z)*(p2->y-p1->y));
    v->y=((p1->z-p0->z)*(p2->x-p1->x))-((p1->x-p0->x)*(p2->z-p1->z));
    v->z=((p1->x-p0->x)*(p2->y-p1->y))-((p1->y-p0->y)*(p2->x-p1->x));
}
// Zustand nach system.c:429/443 (njSetScreen dist 500, 640x480, Mitte 320/240;
// njSetAspect(BHD_ASPECT_X 1.174, BHD_ASPECT_Y 1.0)). Die Vorbelegung von
// fNaViwOffsetX/Y in ps2_NaView.c:14 ist mit cx/cy derselben Aufrufe identisch.
NJS_SCREEN _nj_screen_={500.0f,640.0f,480.0f,320.0f,240.0f};
Float fNaViwOffsetX=2048.0f;
Float fNaViwOffsetY=2048.0f;
Float fNaViwAspectW=1.174000025f;
Float fNaViwAspectH=1.0f;
NJS_MATRIX NaViewScreenMatrix={1.174000025f,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,500.0f};
// Host-Mod Breitbild (widescreen.h): die waagerechte Bildschirmabbildung traegt den Faktor wie
// der GameCube-Patch auf dem gespeicherten Aspekt; 1.0 laesst das Original unveraendert.
float recvx_widescreen_scale=1.0f;
void recvx_set_widescreen(int on){
    recvx_widescreen_scale=on?0.75f:1.0f;
    njViewScreenMatrix(&NaViewScreenMatrix);
}
// ps2_NaView.c:217 njViewScreenMatrix.
void njViewScreenMatrix(NJS_MATRIX* vs){
    Float* p=*vs;
    njUnitMatrix(vs);
    p[0]=fNaViwAspectW*recvx_widescreen_scale;p[5]=fNaViwAspectH;
    p[10]=1.0f;p[12]=0;p[13]=0;p[14]=0;
    p[15]=_nj_screen_.dist;
}
// ps2_NaView.c:27 njSetScreen. Ps2CalcScreenCone, vu1SetScreenProjection und
// vu1SetScreenOffset sind Hardwareschritte ohne nativen Abnehmer; fNaViwHalfW/H
// und ClipDispW/H haben hier keinen Leser.
void njSetScreen(NJS_SCREEN* s){
    _nj_screen_.dist=s->dist;
    _nj_screen_.w=s->w;_nj_screen_.h=s->h;
    _nj_screen_.cx=s->cx;_nj_screen_.cy=s->cy;
    fNaViwOffsetX=s->cx+1728.0f;
    fNaViwOffsetY=s->cy+1808.0f;
    njViewScreenMatrix(&NaViewScreenMatrix);
}
// ps2_NaView.c:81 njSetAspect.
void njSetAspect(Float ax,Float ay){
    fNaViwAspectW=ax;fNaViwAspectH=ay;
    njViewScreenMatrix(&NaViewScreenMatrix);
}
// ps2_NaView.c:55 njSetPerspective: halber Winkel, dann (w/2) / tan. Der
// Faktor 0.0000958738 ist die Originalkonstante. njViewScreenMatrix wird hier
// wie im Original NICHT erneut gerufen, [15] bleibt also stehen.
void njSetPerspective(Angle ang){
    ang/=2;
    _nj_screen_.dist=(_nj_screen_.w/2.0f)/tanf(0.0000958738f*(Float)ang);
}
// ps2_NaMatrix.c:1572 njProjectScreen. njMulMatrixCN rechnet NaViewScreenMatrix
// mal m in die VU-Register, njCalcPointCN liest sie; nativ steht dafuer eine
// lokale Matrix. Die Division durch z bleibt ungeschuetzt wie im Original.
void njProjectScreen(NJS_MATRIX* m,NJS_POINT3* p3,NJS_POINT2* p2){
    NJS_MATRIX combined;NJS_POINT3 point;Float q;
    if(m==NULL)m=pNaMatMatrixStuckPtr;
    njSetMatrix(&combined,&NaViewScreenMatrix);
    njMultiMatrix(&combined,m);
    njCalcPoint(&combined,p3,&point);
    q=_nj_screen_.dist/point.z;
    p2->x=point.x*q+fNaViwOffsetX;
    p2->y=point.y*q+fNaViwOffsetY;
}
/* ps2_NaMatrix.c:1016 njMirror, Ablauf nach der Disassembly (siehe ninja_matrix.h). */
static int float_bits_zero(Float f){uint32_t u;memcpy(&u,&f,sizeof u);return u==0;}
static Angle mirror_angle(Float a,Float b){
    /* njAtan2b (0x002D7210): fptodp beider Argumente, atan2 in double, dptofp. */
    const Float t=(Float)atan2((double)a,(double)b);
    /* vmulx.x mit vf4.x = 32768, vdiv durch vf3.w = 3.141592f, vftoi0 schneidet gegen null ab. */
    return (Angle)((t*32768.0f)/3.141592f);
}
int njMirror(NJS_MATRIX* m,NJS_PLANE* pl){
    NJS_MATRIX mat;Float q,nx,ny,nz,d,tx,ty,tz,s;Angle ax,az;
    const Float vx=pl->vx,vy=pl->vy,vz=pl->vz;
    q=njInvertSqrt((vx*vx+vy*vy)+vz*vz);          /* vmul.xyz, vaddy.x, vaddz.x, vrsqrt */
    nx=vx*q;ny=vy*q;nz=vz*q;                        /* vmulq.xyz vf9 */
    d=((0.0f-nx*pl->px)-ny*pl->py)-nz*pl->pz;       /* vsubx/vsuby/vsubz.x vf10 */
    if(float_bits_zero(d))return 0;                 /* 0x002D7300: Zweig mit jr ra ohne Stapelabbau */
    {const Float t2=d+d;tx=0.0f-vx*t2;ty=0.0f-vy*t2;tz=0.0f-vz*t2;} /* vadd.x vf10; vmulx.xyz vf14; vsub.xyz vf12 */
    if(float_bits_zero((0.0f+vy)+vz)&&float_bits_zero(0.0f-ny*d))return 0; /* 0x002D7340: lAngleX/Z ungeschrieben */
    ax=mirror_angle(vz,vy);
    s=sqrtf(fabsf((0.0f+vy)+vz));                   /* vsqrt Q, vf7x mit vf7.x = vy + vz */
    az=-mirror_angle(vx,s);                         /* vsubx.x vf12, vf0, vf12x vor vftoi0 */
    njUnitMatrix(&mat);
    njUnitRotPortion(&mat);
    mat[12]=tx;mat[13]=ty;mat[14]=tz;mat[15]=1.0f;  /* sqc2 vf12, 0x30(a0); w liest njMultiMatrix nicht (vf0w) */
    njRotateX(&mat,ax);
    njRotateZ(&mat,az);
    njScale(&mat,1.0f,-1.0f,1.0f);
    njRotateZ(&mat,-az);
    njRotateX(&mat,-ax);
    njMultiMatrix(m,&mat);                          /* NULL = pNaMatMatrixStuckPtr wie im Original */
    return 1;
}
