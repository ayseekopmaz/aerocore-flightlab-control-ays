#include "LabTypes.hpp"
#include <stdexcept>
#include <iostream>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(int argc,char**argv){
    if(argc==3&&QString(argv[1])=="--export"){QDir d(argv[2]);QDir().mkpath(d.absolutePath());for(auto k:{"hover","route","takeoff","wind","payload","battery","sensor","link","controller","regression"}){QFile f(d.filePath(QString(k)+".json"));require(f.open(QIODevice::WriteOnly),"preset export file");f.write(QJsonDocument(labPreset(k).json()).toJson());}return 0;}

    for(auto k:{"hover","route","takeoff","wind","payload","battery","sensor","link","controller","regression"}){auto s=labPreset(k);require(s.validate().isEmpty(),k);QString error;auto decoded=LabSpec::fromJson(s.json(),error);require(decoded.has_value()&&decoded->json()==s.json(),"all ten preset round trips");}
    auto s=labPreset("wind");s.events.append({11,3,3,0,"wind"});require(!s.validate().isEmpty(),"overlapping events rejected");
    s=labPreset("hover");auto o=s.json();o["altitude"]="5";QString error;require(!LabSpec::fromJson(o,error),"wrong numeric types rejected");o=s.json();o["schema"]=2;require(!LabSpec::fromJson(o,error),"unknown schemas rejected");
    s.parameters["bad name"]=1;require(!s.validate().isEmpty(),"bad parameter identifiers rejected");
    LabResult r;r.spec=labPreset("route");r.id="sample";r.status="FAIL";r.samples={{1,100,2,3,5,.3,.1,0,0,0,1,70,true}};auto round=LabResult::fromJson(r.json(),error);require(round&&round->json()==r.json(),"result round trip");
    auto malformed=r.json();malformed["samples"]=QJsonArray{QJsonArray{1,"invalid"}};require(!LabResult::fromJson(malformed,error),"invalid sample rejected");std::cout<<"Scenario and result schema tests passed\n";
}
