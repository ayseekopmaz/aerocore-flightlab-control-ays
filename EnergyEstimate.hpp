#pragma once
#include <cmath>
#include <optional>
namespace aero {
// Post-test estimate from supplied hover current, never measured battery telemetry.
// Assumes steady altitude, power proportional to thrust^1.5, and no acceleration.
struct FlightEnergyEstimate {
    double hoverA=0,voltage=0,capacityMah=0,usableFraction=.8;
    double mah=0,wh=0,coveredS=0;
    std::optional<double> lastT,lastA;
    bool configValid()const{return std::isfinite(hoverA)&&hoverA>0&&std::isfinite(voltage)&&voltage>0&&std::isfinite(capacityMah)&&capacityMah>0&&std::isfinite(usableFraction)&&usableFraction>0&&usableFraction<=1;}
    void add(double t,double rollDeg,double pitchDeg,bool fresh){
        double vertical=std::cos(rollDeg*3.141592653589793/180)*std::cos(pitchDeg*3.141592653589793/180);
        if(!configValid()||!fresh||!std::isfinite(t)||!std::isfinite(vertical)||vertical<.5){lastT.reset();lastA.reset();return;}
        double amps=hoverA/std::pow(vertical,1.5);
        if(lastT&&lastA){double dt=t-*lastT;if(dt>0&&dt<=1){double mean=(amps+*lastA)*.5;mah+=mean*dt/3.6;wh+=mean*voltage*dt/3600;coveredS+=dt;}}
        lastT=t;lastA=amps;
    }
    double usableRemainingMah()const{return capacityMah*usableFraction-mah;}
};
}
