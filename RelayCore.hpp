#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
namespace aura {
struct Battery {
    double capacityMah=1300,hoverA=9.8,voltageV=21.9,chargeA=1.3,soc=1,usedMah=0;
    double currentA=0,chargedMah=0;
    bool update(double dt,bool airborne,bool charging,double roll,double pitch){
        if(!std::isfinite(dt)||dt<=0||dt>1||capacityMah<=0||hoverA<=0)return false;
        if(airborne){double vertical=std::cos(roll*3.141592653589793/180)*std::cos(pitch*3.141592653589793/180);if(!std::isfinite(vertical)||vertical<.5){currentA=0;return false;}currentA=hoverA/std::pow(vertical,1.5);double mah=currentA*dt/3.6;usedMah+=mah;soc=std::max(0.,soc-mah/capacityMah);return true;}
        currentA=charging?-chargeA:0;if(charging){double mah=std::min(capacityMah*(1-soc),chargeA*dt/3.6);chargedMah+=mah;soc=std::min(1.,soc+mah/capacityMah);}return true;
    }
};
struct Coverage {
    double durationS=0,coveredS=0,gapS=0,longestGapS=0,currentGapS=0,overlapS=0;
    void update(double dt,int observers){if(!std::isfinite(dt)||dt<=0||dt>1)return;durationS+=dt;if(observers>0){coveredS+=dt;currentGapS=0;if(observers>1)overlapS+=dt;}else {gapS+=dt;currentGapS+=dt;longestGapS=std::max(longestGapS,currentGapS);}}
    double percent()const{return durationS>0?100*coveredS/durationS:0;}
};
// Only accept takeover after fresh in-zone confirmation sustained for dwell seconds.
struct HandoverGate {
    std::optional<double> insideSince;
    bool update(double now,bool freshInZone,double dwell=2){if(!freshInZone){insideSince.reset();return false;}if(!insideSince)insideSince=now;return now-*insideSince>=dwell;}
    void reset(){insideSince.reset();}
};
inline bool needsRelief(double soc,double trigger){return std::isfinite(soc)&&soc<=trigger;}
}
