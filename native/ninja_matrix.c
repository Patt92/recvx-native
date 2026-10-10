// Host translation of the affine matrix/vector operations in ps2_NaMatrix.c.
// Column-major storage and rotation signs follow the original VU operations.
#include "ninja_matrix.h"
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
