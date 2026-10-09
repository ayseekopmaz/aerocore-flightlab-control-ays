#include "ExperimentCore.hpp"
#include <stdexcept>
#include <iostream>
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){
    require(std::abs(aero::segmentError({5,3},{0,0},{10,0})-3)<1e-9,"cross track distance");
    require(std::abs(aero::segmentError({12,0},{0,0},{10,0})-2)<1e-9,"clamp beyond endpoint");
    auto g=aero::geographic({12,34},47.4,8.54);auto p=aero::local(g.north,g.east,47.4,8.54);require(aero::distance(p,{12,34})<1e-6,"geography round trip");
    aero::Stats stats;stats.add(1);stats.add(2);stats.add(3);require(stats.mean==2&&stats.maximum==3&&std::abs(*stats.deviation()-1)<1e-9,"Welford sample variance");
    aero::Energy energy;energy.add(1,100);energy.add(2,100);energy.add(3,{});energy.add(10,100);energy.add(11,100);require(std::abs(energy.wh-200./3600)<1e-12&&energy.covered==2,"energy does not integrate unavailable data / gaps");
    require(aero::result(true,true,false)=="PASS"&&aero::result(true,true,true)=="FAIL"&&aero::result(true,false,false)=="INCONCLUSIVE"&&aero::result(false,true,false)=="INCONCLUSIVE","never pass incomplete measurements");
    std::cout<<"Experiment math / coverage / energy tests passed\n";
}
