#include "SessionRecording.hpp"
#include "FleetPanel.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <cstdlib>
#include <iostream>
void check(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir dir;auto path=dir.filePath("record.json");SessionRecording record;double now=sitl::monoSeconds();FlightState s;
    for(int n=1;n<=3;++n){auto&v=s.vehicles[n];v.lat=47+n*.0001;v.lon=8;v.alt=n;v.roll=n*3;v.pitch=n;v.yaw=n*20;v.battery=90;v.armed=true;v.px4=true;v.landed=2;v.posAt=v.attitudeAt=v.landedAt=v.batteryAt=v.heartbeatAt=now;}
    check(record.start(path,{{"firmware","test"}}),"record start");check(!record.start(path,{}),"double start accepted");record.event("test event",now+.05);record.capture(s,now+.1);s.vehicles[2].alt=12;record.capture(s,now+.3);QString error;check(record.save(error),"record save");auto json=SessionRecording::load(path,error);check(bool(json),"record load");check((*json)["frames"].toArray().size()==2,"frame count");FlightState played;check(SessionRecording::applyFrame((*json)["frames"].toArray().last().toObject(),played,now+10),"apply frame");check(played.vehicles.size()==3&&played.vehicles[2].alt==12&&played.vehicles[1].roll==3,"identity/pose lost");check(std::abs(played.vehicles[2].posAt-(now+9.7))<.001,"freshness age lost");
    auto bad=(*json)["frames"].toArray().first().toObject();auto vs=bad["vehicles"].toArray();vs.append(vs[0]);bad["vehicles"]=vs;check(!SessionRecording::applyFrame(bad,played,now),"duplicate SYS accepted");
    auto corrupted=*json;auto frames=corrupted["frames"].toArray();frames[0]=frames[1];frames[1]=(*json)["frames"].toArray()[0];corrupted["frames"]=frames;QSaveFile f(path);check(f.open(QIODevice::WriteOnly),"open bad JSON");f.write(QJsonDocument(corrupted).toJson());f.commit();check(!SessionRecording::load(path,error),"out-of-order frames accepted");
    VehicleProfile v;v.system=2;v.name="B";v.physical["payload"]=.2;v.parameters["MPC_XY_P"]=.9;auto parsed=VehicleProfile::fromJson(v.json(),error);check(bool(parsed),"profile parse");LabSpec spec;parsed->apply(spec);check(spec.payload==.2&&spec.parameters["MPC_XY_P"]==.9,"profile application");auto invalid=v.json();auto params=invalid["parameters"].toObject();params["MAV_SYS_ID"]=3;invalid["parameters"]=params;check(!VehicleProfile::fromJson(invalid,error),"reserved SYS override accepted");
    std::cout<<"Recording roundtrip, replay freshness, malformed timeline and vehicle profiles verified\n";
}
