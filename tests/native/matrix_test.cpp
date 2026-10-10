#include "ninja_matrix.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<0.001f;}
static void point(NJS_MATRIX* m,NJS_VECTOR input,NJS_VECTOR expected){
    njCalcPoint(m,&input,&input);
    check(near(input.x,expected.x)&&near(input.y,expected.y)&&near(input.z,expected.z),"matrix point/order mismatch");
}
int main(){try{
    _Make_SinTable();NJS_MATRIX m;
    njUnitMatrix(&m);njTranslate(&m,2,3,4);njScale(&m,2,3,4);
    point(&m,{1,2,3},{4,9,16});
    NJS_VECTOR v{1,2,3};njCalcVector(&m,&v,&v);check(near(v.x,2)&&near(v.y,6)&&near(v.z,12),"vector includes translation");
    check(njInvertMatrix(&m),"invert nonsingular matrix");point(&m,{4,9,16},{1,2,3});
    njUnitMatrix(&m);njRotateX(&m,16384);point(&m,{0,1,0},{0,0,1});
    njUnitMatrix(&m);njRotateY(&m,16384);point(&m,{0,0,1},{1,0,0});
    njUnitMatrix(&m);njRotateZ(&m,16384);point(&m,{1,0,0},{0,1,0});
    njUnitMatrix(&m);njRotateZ(&m,16384);njTranslate(&m,2,0,0);point(&m,{0,0,0},{0,2,0});
    NJS_MATRIX source;njUnitMatrix(&source);njTranslate(&source,0,2,0);njMultiMatrix(&m,&source);point(&m,{0,0,0},{-2,2,0});
    NJS_MATRIX singular{};check(!njInvertMatrix(&singular),"singular matrix accepted");
    for(auto f:singular)check(f==0,"failed inverse mutated input");
    NJS_MATRIX stack[3];njInitMatrix(stack,3,0);njTranslate(nullptr,1,2,3);
    check(njPushMatrixEx(),"push1");njTranslate(nullptr,2,0,0);check(njPushMatrixEx(),"push2");
    check(!njPushMatrixEx(),"stack overflow accepted");check(!njPopMatrix(3),"stack underflow accepted");
    check(njPopMatrix(2),"pop2");point(nullptr,{0,0,0},{1,2,3});
    njUnitMatrix(&NaViwViewMatrix);njTranslate(&NaViwViewMatrix,5,6,7);njClearMatrix();point(nullptr,{0,0,0},{5,6,7});
    njUnitMatrix(&NaViwViewMatrix);njInitMatrix(nullptr,0,0);
    NJS_VECTOR a{1,0,0},b{0,1,0};check(near(njOuterProduct(&a,&b,&a),1)&&near(a.z,1),"in-place cross product");
    /* njRotate um eine freie Achse (ps2_NaMatrix.c:737). Die Originalroutine
       geht ueber den halben Winkel und quadriert; njRotateX/Y/Z tasten den
       vollen Winkel direkt ab. Beide Wege unterscheiden sich daher um bis zu
       einen Schritt der Sinustabelle (16384 Eintraege), nicht mehr. */
    {
        const auto worst=[](NJS_MATRIX* x,NJS_MATRIX* y){
            float d=0;
            for(int i=0;i<16;++i){const float e=std::abs((*x)[i]-(*y)[i]);if(e>d)d=e;}
            return d;
        };
        NJS_MATRIX free_axis,reference;NJS_VECTOR axis;
        /* Auf den Hauptachsen muss njRotate die drei Einzelroutinen treffen. */
        for(int step=1;step<8;++step){
            const Angle angle=static_cast<Angle>(0x10000*step/8);
            njUnitMatrix(&free_axis);axis={1,0,0};njRotate(&free_axis,&axis,angle);
            njUnitMatrix(&reference);njRotateX(&reference,angle);
            check(worst(&free_axis,&reference)<0.001f,"njRotate on the x axis differs from njRotateX");
        }
        njUnitMatrix(&free_axis);axis={0,1,0};njRotate(&free_axis,&axis,16384);
        njUnitMatrix(&reference);njRotateY(&reference,16384);
        check(worst(&free_axis,&reference)<0.001f,"njRotate on the y axis differs from njRotateY");
        njUnitMatrix(&free_axis);axis={0,0,1};njRotate(&free_axis,&axis,16384);
        njUnitMatrix(&reference);njRotateZ(&reference,16384);
        check(worst(&free_axis,&reference)<0.001f,"njRotate on the z axis differs from njRotateZ");
        /* :762 die Achse wird in der Routine normiert; jede Laenge ist zulaessig. */
        njUnitMatrix(&free_axis);axis={0,7.5f,0};njRotate(&free_axis,&axis,16384);
        njUnitMatrix(&reference);njRotateY(&reference,16384);
        check(worst(&free_axis,&reference)<0.001f,"njRotate does not normalise the axis");
        /* :745 ang wird auf 16 Bit gemaskt, DANN halbiert: -90 ist 0xC000. */
        njUnitMatrix(&free_axis);axis={0,1,0};njRotate(&free_axis,&axis,-16384);
        njUnitMatrix(&reference);axis={0,1,0};njRotate(&reference,&axis,0xC000);
        check(worst(&free_axis,&reference)==0.0f,"njRotate negative angle is not the masked angle");
        /* Verkettung und Orthonormalitaet bei schraeger Achse. */
        njUnitMatrix(&free_axis);axis={1,1,0};njRotate(&free_axis,&axis,8192);njRotate(&free_axis,&axis,8192);
        njUnitMatrix(&reference);axis={1,1,0};njRotate(&reference,&axis,16384);
        check(worst(&free_axis,&reference)<0.001f,"njRotate does not compose");
        njUnitMatrix(&free_axis);axis={1,2,3};njRotate(&free_axis,&axis,0x2345);
        for(int column=0;column<3;++column){
            float length=0;
            for(int row=0;row<3;++row)length+=free_axis[column*4+row]*free_axis[column*4+row];
            check(near(std::sqrt(length),1),"njRotate column is not unit length");
        }
        /* :813 Rechtsmultiplikation wie njRotateX/Y/Z: eine bestehende
           Translation bleibt stehen, und die vierte Zeile wird nicht beruehrt. */
        njUnitMatrix(&free_axis);njTranslate(&free_axis,10,0,0);
        axis={0,1,0};njRotate(&free_axis,&axis,16384);
        check(near(free_axis[12],10)&&near(free_axis[13],0)&&near(free_axis[14],0),"njRotate moved the translation column");
        njUnitMatrix(&free_axis);free_axis[3]=5;free_axis[7]=6;free_axis[11]=7;
        axis={1,2,3};njRotate(&free_axis,&axis,0x1234);
        check(free_axis[3]==5&&free_axis[7]==6&&free_axis[11]==7,"njRotate wrote the fourth row");
    }
    /* njArcSin (macros.h:108) mit der Originalkonstante PI_2 = 3.141592f*2. */
    check((njArcSin(1)&0xFFFF)==0x4000,"njArcSin(1) is not a quarter turn");
    check((njArcSin(-1)&0xFFFF)==0xC000,"njArcSin(-1) is not three quarter turns");
    check(njArcSin(0)==0&&(njArcSin(0.5f)&0xFFFF)==0x1555,"njArcSin scale");
    std::cout<<"Native matrix tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
