#pragma once
#include <array>
#include <cmath>
namespace shield {
struct Target{double east,north,alt;};
inline std::array<Target,3> targets(double center,int leader,bool adapted,double altitude=8){
    const double back=5*std::cos(3.141592653589793/6),side=5*std::sin(3.141592653589793/6);
    std::array<Target,3> t{};t[leader]={center,0,altitude};int follower=0;
    for(int n=0;n<3;++n)if(n!=leader){t[n]={center-back,follower++==0?-side:side,adapted&&n==0?altitude-2:altitude};}return t;
}
inline bool stressed(double current,double baseline){return std::isfinite(current)&&std::isfinite(baseline)&&baseline>0&&current>=baseline*1.4;}
inline double saving(double baseline,double adaptive){return baseline>0?(baseline-adaptive)/baseline*100:0;}
}
