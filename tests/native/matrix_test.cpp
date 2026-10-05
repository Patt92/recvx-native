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
    std::cout<<"Native matrix tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
