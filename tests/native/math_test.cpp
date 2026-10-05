#include "ninja_math.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static bool near(float x, float y, float eps=0.00001f) { return std::abs(x-y)<=eps; }
int main() {
    try {
        _Make_SinTable();
        constexpr double pi=3.14159265358979323846;
        for (int angle=0; angle<65536; ++angle) {
            const double radians=angle*(2*pi/65536);
            check(near(njSin(angle),static_cast<float>(std::sin(radians)),0.00011f),"sine quadrant/lookup mismatch");
            check(near(njCos(angle),static_cast<float>(std::cos(radians)),0.00011f),"cosine quadrant/lookup mismatch");
            float s,c;
            njSinCos(angle,&s,&c);
            check(s==njSin(angle) && c==njCos(angle),"SinCos disagrees with scalar calls");
        }
        check(njSin(-1234)==njSin(65536-1234),"negative angle wrapping");
        check(njCos(65536+1234)==njCos(1234),"large angle wrapping");
        check(near(njFraction(-1.25f),0.75f),"negative fraction");
        check(near(njSqrt(9),3) && near(njSqrt(-9),3),"VU square root sign behavior");
        check(near(njInvertSqrt(4),0.5f),"inverse square root");
        check(njInvertSqrt(0)==std::numeric_limits<float>::max(),"inverse sqrt saturation");
        check(njInvertSqrt(-0.0f)==-std::numeric_limits<float>::max(),"inverse sqrt signed zero");
        float linear[]{0,2,4,10,12,14}, output[3]{};
        njLinear(linear,output,nullptr,0.25f);
        check(near(output[0],2.5f)&&near(output[1],4.5f)&&near(output[2],6.5f),"linear interpolation");
        njLinear(linear,linear,nullptr,0.25f);
        check(near(linear[2],6.5f),"linear overlapping input/output");
        float spline[]{0,0,0,2,4,6,4,8,12,6,12,18};
        njOverhauserSpline(spline,output,nullptr,0);
        check(near(output[0],2)&&near(output[2],6),"spline start endpoint");
        njOverhauserSpline(spline,output,nullptr,1);
        check(near(output[0],4)&&near(output[2],12),"spline end endpoint");
        njOverhauserSpline(spline,output,nullptr,0.5f);
        check(near(output[0],3)&&near(output[2],9),"spline midpoint");
        int count=3;
        NJS_SPLINE attr{0,&count,nullptr};
        float bezier[]{0,0,0,2,4,6,8,8,8};
        njBezierSpline(bezier,output,&attr,0.5f);
        check(near(output[0],3)&&near(output[1],4)&&near(output[2],5),"Bezier curve");
        check(njFactorial(0)==1 && njFactorial(5)==120,"factorial");
        std::cout << "Native game math tests passed (all 65536 angles)\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
