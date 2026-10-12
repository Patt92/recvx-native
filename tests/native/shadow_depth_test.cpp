#include "shadow_depth.h"
#include "shadow_transport.h"
#include <iostream>
#include <limits>
#include <stdexcept>

static void check(bool ok,const char* reason){
    if(!ok)throw std::runtime_error(reason);
}
int main(){
    try{
        using namespace recvx;
        check(shadow_packed_vertex_depth(-1.f)==0,"lower saturation");
        check(shadow_packed_vertex_depth(0.f)==0,"zero");
        check(shadow_packed_vertex_depth(.999f)==0,"fraction not discarded");
        check(shadow_packed_vertex_depth(1.f)==1,"integer boundary");
        check(shadow_packed_vertex_depth(1.9375f)==1,"transport fraction retained");
        check(shadow_packed_vertex_depth(65534.f)==65534,"upper endpoint");
        check(shadow_packed_vertex_depth(70000.f)==65534,"upper saturation");

        // Independently selected exact boundary, not a tolerance comparison.
        const float below=std::nextafter(4096.f,0.f);
        check(shadow_packed_vertex_depth(below)==4095,"below integer boundary");
        check(shadow_packed_vertex_depth(4096.f)==4096,"at integer boundary");

        // Signed near=-1, far=-2 gives exact binary coefficients:
        // original a=-65536, b=-131072.
        const auto c=shadow_depth_coefficients(-1.f,-2.f);
        check(c.offset==65536.f&&c.scale==131072.f,"coefficient signs/order");
        check(shadow_vertex_depth(c,1.f)==65534,"near transport saturation");
        check(shadow_vertex_depth(c,.5f)==65534,"far transport saturation");

        // The ELF confirms positive fQ=1/view_z and D=-a-b*fQ.
        // Keep this short clip interval's saturation visible; it is NOT
        // representative of the actual 2/20000 game clip interval.
        std::cout<<"diagnostic signed[-1,-2]: offset="<<c.offset
                 <<" scale="<<c.scale
                 <<" z=1:"<<shadow_vertex_depth(c,1.f)
                 <<" z=2:"<<shadow_vertex_depth(c,.5f)<<"\n";

        const auto limited=shadow_depth_coefficients(0.f,-2.f);
        check(limited.offset==c.offset&&limited.scale==c.scale,
              "original near limit differs");

        // Ps2AddPrim else-branch, ps2_dummy.c:432: sz = a + b / z with the
        // raw CalcPs2ZbuffAB coefficients. For signed near=-1, far=-2 the
        // producer yields exactly a=-65536, b=-131072 (ps2_NaDraw2D.c:12-13).
        check(shadow_vertex_depth_linear(-65536.f,-131072.f,1.f)==0,
              "linear branch near saturation");
        check(shadow_vertex_depth_linear(-65536.f,-131072.f,2.f)==0,
              "linear branch far saturation");
        check(shadow_vertex_depth_linear(100.f,200.f,2.f)==200,
              "linear branch arithmetic");
        check(shadow_vertex_depth_linear(0.0625f,0.f,1.f)==0,
              "linear branch kept a discarded transport fraction");
        check(shadow_vertex_depth_linear(70000.f,0.f,1.f)==65534,
              "linear branch upper saturation");
        bool linear_rejected=false;
        try{(void)shadow_vertex_depth_linear(1.f,1.f,0.f);}
        catch(const std::runtime_error&){linear_rejected=true;}
        check(linear_rejected,"linear branch accepted zero view Z");
        linear_rejected=false;
        try{(void)shadow_vertex_depth_linear(
            1.f,1.f,std::numeric_limits<float>::quiet_NaN());}
        catch(const std::runtime_error&){linear_rejected=true;}
        check(linear_rejected,"linear branch accepted nonfinite view Z");

        // Actual game defaults. These integers are independently calculated
        // from the ELF coefficients with Float32 intermediates, not inferred
        // from the diagnostic [-1,-2] saturation.
        const auto game=shadow_depth_coefficients(-2.f,-20000.f);
        check(game.offset==6.554255485534668f&&game.scale==131085.109375f,
              "game coefficient bits/order changed");
        const float z_values[]={1,2,4,10,100,1000,20000};
        const unsigned expected[]={65534,65534,32777,13115,1317,137,13};
        for(unsigned i=0;i<7;++i)
            check(shadow_vertex_depth(game,1.f/z_values[i])==expected[i],
                  "positive reciprocal-Z transport changed");
        check(shadow_depth_pass(137,137),"GEQUAL must retain a quantized tie");
        check(!shadow_depth_pass(136,137),"behind receiver passed GEQUAL");
        check(shadow_depth_pass(138,137),"nearer modifier failed GEQUAL");
        check(shadow_depth_pass(65534,65534),"saturated tie rejected");

        // Transport BEFORE interpolation is observable. At the midpoint of
        // vertex D={1.9375,2.9375}, interpolating transported {1,2} yields 1.5;
        // interpolating raw D and then packing yields 2. It cannot be obtained
        // by converting the renderer's completed reciprocal-depth buffer.
        const float transported_middle=(shadow_packed_vertex_depth(1.9375f)
                                        +shadow_packed_vertex_depth(2.9375f))*.5f;
        check(transported_middle==1.5f,"vertex transport order changed");
        check(shadow_packed_vertex_depth(2.4375f)==2,
              "diagnostic interpolation distinction disappeared");

        // The guard's VADDA is independent of the following depth VADDA.
        // Equal reciprocal Z gives equal D even when raw guard z differs.
        ShadowProjectedVertex v{2048,2048,10,.1f};
        const auto original=transport_shadow_vertex(v,game);
        v.z=4000;
        const auto guarded=transport_shadow_vertex(v,game);
        check(original.depth==guarded.depth,"guard accumulator leaked into depth");
        check(!original.outside&&guarded.outside,"original far guard missing");
        v.z=1;
        check(!transport_shadow_vertex(v,game).outside,"near guard equality rejected");
        v.z=.5f;
        check(transport_shadow_vertex(v,game).outside,"near guard rejected incorrectly");
        v.z=10;v.x=4095;
        check(!transport_shadow_vertex(v,game).outside,"XY guard equality rejected");
        v.x=std::nextafter(4095.f,5000.f);
        check(transport_shadow_vertex(v,game).outside,"strict XY guard missing");
        v.x=2048;v.y=1;
        check(!transport_shadow_vertex(v,game).outside,"negative XY guard equality rejected");
        // Use a representable guard difference: subtracting 2048 would
        // absorb nextafter(1,0), as it does in the original float operation.
        v.y=.5f;
        check(transport_shadow_vertex(v,game).outside,"negative XY guard missing");
        check(shadow_transport_triangle_suppressed({original,guarded,original}),
              "one rejected corner did not suppress final triangle kick");
        check(!shadow_transport_triangle_suppressed({original,original,original}),
              "accepted triangle suppressed");

        bool rejected=false;
        try{(void)shadow_depth_coefficients(-2.f,-1.f);}
        catch(const std::runtime_error&){rejected=true;}
        check(rejected,"invalid clip ordering accepted");

        rejected=false;
        try{(void)shadow_packed_vertex_depth(
            std::numeric_limits<float>::infinity());}
        catch(const std::runtime_error&){rejected=true;}
        check(rejected,"nonfinite depth accepted");

        std::cout<<"PASS: transport bits, truncation, saturation and input checks\n";
        std::cout<<"ELF-backed positive reciprocal-Z, ACC overwrite and guard; no renderer acceptance\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
