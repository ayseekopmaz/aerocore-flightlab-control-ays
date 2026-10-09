#include "LabTransport.hpp"
#include "SimulatorSession.hpp"
#include "SimulationProtocol.hpp"
#include <QCoreApplication>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <iostream>
#include <cstdlib>
struct Session:SimulatorSession{bool owned()const override{return true;}bool running()const override{return true;}};
void check(bool x,const char*m){if(!x){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto state=std::make_shared<FlightState>();state->peer="127.0.0.1";Session session;session.setFleet(QJsonArray{QJsonObject{},QJsonObject{},QJsonObject{}});QUdpSocket source,a,b,c;check(source.bind(QHostAddress::LocalHost,quint16(0)),"source bind");check(a.bind(QHostAddress::LocalHost,14561)&&b.bind(QHostAddress::LocalHost,14562)&&c.bind(QHostAddress::LocalHost,14563),"fleet ports occupied");LabTransport transport(state,&session,&source,nullptr);
    state->selected=2;auto cmd=sitl::commandLong(400,2,1,{1,0,0,0,0,0,0},0);check(transport.send(cmd),"send");check(b.waitForReadyRead(500),"SYS2 did not receive");check(b.receiveDatagram().data()==QByteArray(reinterpret_cast<const char*>(cmd.data()),cmd.size()),"bytes");check(!a.hasPendingDatagrams()&&!c.hasPendingDatagrams(),"command leaked to another SYS");
    auto heartbeat=sitl::gcsHeartbeat(1);check(transport.heartbeat(heartbeat),"heartbeat broadcast");for(auto*s:{&a,&b,&c}){if(!s->hasPendingDatagrams())s->waitForReadyRead(500);check(s->hasPendingDatagrams(),"missing per-vehicle heartbeat");s->receiveDatagram();}
    LabAction fault;fault.type="link";fault.a=0;fault.b=100;check(transport.issue(fault),"delay issue");QCoreApplication::processEvents();check(transport.send(cmd),"delay send");state->selected=3;transport.tick(sitl::monoSeconds()+1);check(b.waitForReadyRead(500),"delayed packet rerouted after SYS switch");b.receiveDatagram();check(!c.hasPendingDatagrams(),"delayed packet leaked");transport.resetFault();
    state->selected=2;fault.a=100;fault.b=0;check(transport.issue(fault),"loss issue");QCoreApplication::processEvents();QNetworkDatagram in(QByteArray("test"));in.setSender(QHostAddress::LocalHost,14562);check(transport.receive(in,sitl::monoSeconds()).isEmpty(),"SYS2 loss not applied");in.setSender(QHostAddress::LocalHost,14563);check(transport.receive(in,sitl::monoSeconds()).size()==1,"SYS3 affected by SYS2 fault");
    std::cout<<"Per-SYS routing, heartbeat fanout, delayed destination and isolated link fault verified\n";
}
