#pragma once
#include <math.h>
// SDK sphere/capsule and capsule/capsule geometry on finite centre segments.
// Returns closest points and squared distance; degenerate segments become points.
static float recvx_segment_closest(const float a[3],const float b[3],const float c[3],const float d[3],float pa[3],float pb[3]){
    float u[3],v[3],w[3],aa=0,bb=0,cc=0,dd=0,ee=0,s=0,t=0;
    for(int i=0;i<3;++i){u[i]=b[i]-a[i];v[i]=d[i]-c[i];w[i]=a[i]-c[i];aa+=u[i]*u[i];bb+=u[i]*v[i];cc+=v[i]*v[i];dd+=u[i]*w[i];ee+=v[i]*w[i];}
    if(aa<=1e-12f&&cc<=1e-12f){}
    else if(aa<=1e-12f)t=fmaxf(0,fminf(1,ee/cc));
    else if(cc<=1e-12f)s=fmaxf(0,fminf(1,-dd/aa));
    else{const float denom=aa*cc-bb*bb;s=denom>1e-12f?fmaxf(0,fminf(1,(bb*ee-cc*dd)/denom)):0;t=(bb*s+ee)/cc;if(t<0){t=0;s=fmaxf(0,fminf(1,-dd/aa));}else if(t>1){t=1;s=fmaxf(0,fminf(1,(bb-dd)/aa));}}
    float distance=0;for(int i=0;i<3;++i){pa[i]=a[i]+s*u[i];pb[i]=c[i]+t*v[i];const float q=pa[i]-pb[i];distance+=q*q;}return distance;
}
