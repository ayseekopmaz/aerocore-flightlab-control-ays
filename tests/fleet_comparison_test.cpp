#include "LabPanel.hpp"
#include "FleetPanel.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QUdpSocket>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QTableWidget>
#include <iostream>
#include <stdexcept>
void require(bool v,const char*m){if(!v)throw std::runtime_error(m);}
class TestSession:public SimulatorSession{public:bool running()const override{return true;}bool owned()const override{return true;}QString profileHash()const override{return "test-physical-profile";}QString firmware()const override{return "test-firmware";}QJsonObject labProfile()const override{return LabSpec{}.profile();}};
class TestTransport:public LabTransport{public:using LabTransport::LabTransport;bool pending=false;LabAction next;QMap<int,QMap<QString,double>>params;QVector<int>arms;
    bool issue(const LabAction&a)override{require(!pending,"overlapping actions");next=a;pending=true;return true;}
    void resetFault()override{}void cancel()override{pending=false;}
    void ack(FlightState&s){auto a=next;auto&v=s.vehicles[s.selected];if(a.type=="command"){if(a.command==400){v.armed=a.params[0]==1;if(v.armed)arms.append(s.selected);}if(a.command==22){v.alt=5;v.amsl=505;v.landed=2;}if(a.command==21){v.alt=0;v.amsl=500;v.landed=1;}}
        double value=a.type=="read"?params[s.selected].value(a.name,.95):a.a;if(a.type=="set")params[s.selected][a.name]=a.a;pending=false;emit completed(true,"test ACK / readback",value);}
};
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir home;qputenv("XDG_DATA_HOME",home.path().toUtf8());QCoreApplication::setApplicationName("AeroCoreFleetComparisonTest");auto s=std::make_shared<FlightState>();s->peer="127.0.0.1";TestSession session;QJsonArray profiles;for(int n=1;n<=3;++n){VehicleProfile p;p.system=n;p.name=QString("Vehicle %1").arg(n);p.parameters["MPC_XY_P"]=.8+n*.1;profiles.append(p.json());auto&v=s->vehicles[n];v.px4=true;v.landed=1;v.lat=47;v.lon=8+n*.0001;v.amsl=500;}session.setFleet(profiles);QUdpSocket socket;TestTransport transport(s,&session,&socket,nullptr);LabPanel panel(s,&session,&transport,nullptr);panel.findChild<QDoubleSpinBox*>("durationSpin")->setValue(5);auto*runner=panel.findChild<ExperimentRunner*>();QVector<LabResult>results;QObject::connect(runner,&ExperimentRunner::finished,[&](const LabResult&r){results.append(r);});double now=sitl::monoSeconds();
    auto fresh=[&]{for(auto&v:s->vehicles){v.heartbeatAt=v.posAt=v.attitudeAt=v.landedAt=now;v.bootMs=quint32(now*1000);}};fresh();panel.compareFleet();require(panel.busy(),"comparison did not start");
    for(int step=0;step<2500&&panel.busy();++step){if(transport.pending)transport.ack(*s);now+=.1;fresh();panel.tick(now);QCoreApplication::processEvents();}
    require(!panel.busy(),"comparison did not finish");require(results.size()==3,"comparison result count");require(transport.arms==QVector<int>{1,2,3},"wrong SYS arm ordering");for(int i=0;i<3;++i){require(results[i].status=="PASS","valid comparison flight must pass");require(results[i].metrics["systemId"].toInt()==i+1,"result identity lost");require(results[i].spec.parameters["MPC_XY_P"]==.8+(i+1)*.1,"vehicle controller profile lost");require(!s->vehicles[i+1].armed&&s->vehicles[i+1].landed==1,"final landing/disarm");require(std::abs(transport.params[i+1]["MPC_XY_P"]-.95)<1e-6,"original controller value not restored");}
    auto*table=panel.findChild<QTableWidget*>("resultsTable");require(table->selectionModel()->selectedRows().size()==3,"comparison results not selected together");QDir d(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/experiments");require(d.entryList({"*.json"},QDir::Files).size()==3&&d.entryList({"*.html"},QDir::Files).size()==3,"automatic JSON/HTML reports missing");std::cout<<"Three vehicle experiments, profile application, restoration, comparison selection and automatic reports verified\n";
}
