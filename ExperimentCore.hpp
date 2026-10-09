#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
namespace aero {
constexpr double earth=6371000.0, pi=3.141592653589793;
struct Point { double east=0,north=0; };
inline Point local(double lat,double lon,double lat0,double lon0){return {(lon-lon0)*pi/180*earth*std::cos(lat0*pi/180),(lat-lat0)*pi/180*earth};}
inline Point geographic(Point p,double lat0,double lon0){return {lon0+p.east/(earth*std::cos(lat0*pi/180))*180/pi,lat0+p.north/earth*180/pi};}
inline double distance(Point a,Point b={}){return std::hypot(a.east-b.east,a.north-b.north);}
inline double segmentError(Point p,Point a,Point b){double x=b.east-a.east,y=b.north-a.north,q=x*x+y*y;double t=q>1e-12?std::clamp(((p.east-a.east)*x+(p.north-a.north)*y)/q,0.,1.):0;return distance(p,{a.east+t*x,a.north+t*y});}
struct Stats { std::size_t count=0;double mean=0,m2=0,maximum=0; void add(double v){if(!std::isfinite(v))return;++count;double d=v-mean;mean+=d/count;m2+=d*(v-mean);maximum=std::max(maximum,v);} std::optional<double> deviation()const{if(count<2)return {};return std::sqrt(m2/(count-1));} };
// Integrate only successive fresh power samples. Never bridge outages or accept unknown current.
struct Energy {std::optional<double> lastTime,lastPower;double wh=0,covered=0;void add(double t,std::optional<double> watts){if(watts&&std::isfinite(*watts)&&*watts>=0&&lastTime&&lastPower){double dt=t-*lastTime;if(dt>0&&dt<=2.5){wh+=(*watts+*lastPower)*.5*dt/3600;covered+=dt;}}lastTime=t;lastPower=watts;} };
inline std::string result(bool complete,bool enough,bool exceeded){if(!complete||!enough)return "INCONCLUSIVE";return exceeded?"FAIL":"PASS";}
}
