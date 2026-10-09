#include "ExperimentRunner.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QUdpSocket>
#include <stdexcept>
#include <iostream>
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
class TestSession:public SimulatorSession {
public:
    using SimulatorSession::SimulatorSession;bool active_=true;
    bool running()const override{return active_;}bool owned()const override{return true;}
    QJsonObject labProfile()const override{return LabSpec{}.profile();}QString profileHash()const override{return "test-profile";}QString firmware()const override{return "test-firmware";}
};
class TestTransport:public LabTransport {
public:
    using LabTransport::LabTransport;QVector<LabAction> commands;bool pending=false;LabAction next;QMap<QString,double> values{{"MPC_XY_P",.95}};
    bool issue(const LabAction&a)override{require(!pending,"no overlapping asynchronous actions");commands.append(a);next=a;pending=true;return true;}
    void resetFault()override{}void cancel()override{pending=false;}
    void acknowledge(bool ok=true){require(pending,"ack must have matching request");pending=false;double value=next.type=="read"?values.value(next.name):next.a;if(ok&&next.type=="set")values[next.name]=next.a;emit completed(ok,"test transport evidence",value);}
};
struct Harness {
    std::shared_ptr<FlightState> state=std::make_shared<FlightState>();TestSession session;QUdpSocket socket;TestTransport transport{state,&session,&socket,nullptr};ExperimentRunner runner{state,&session,&transport,nullptr};QVector<LabResult> results;double now=sitl::monoSeconds();
    Harness(){auto&v=state->vehicles[1];v.px4=true;v.lat=47.4;v.lon=8.54;v.amsl=500;v.landed=1;fresh();QObject::connect(&runner,&ExperimentRunner::finished,[this](const LabResult&r){results.append(r);});}
    void fresh(){auto&v=state->vehicles[1];v.heartbeatAt=v.posAt=v.attitudeAt=v.landedAt=now;v.bootMs=std::uint32_t((now-100)*1000);}
    void step(double dt=.1){now+=dt;fresh();runner.tick(now);}
    void successAction(){auto a=transport.next;if(a.type=="command"){auto&v=state->vehicles[1];if(a.command==400)v.armed=a.params[0]==1;if(a.command==22){v.alt=5;v.amsl=505;v.landed=2;}if(a.command==21){v.alt=0;v.amsl=500;v.landed=1;}}transport.acknowledge();}
    void complete(int max=500){for(int i=0;i<max&&runner.busy();++i){if(transport.pending)successAction();step();}require(!runner.busy(),"bounded finish");}
};
int main(int argc,char**argv){QCoreApplication app(argc,argv);
    {Harness h;QString error;auto s=labPreset("hover");s.duration=5;s.maxTime=60;require(h.runner.start({s},error),"hover start");h.complete();require(h.results.size()==1&&h.results[0].status=="PASS","hover requires confirmed flight / landing");require(h.results[0].metrics["samples"].toInt()>=10,"actual position sample coverage");}
    {Harness h;QString error;auto s=labPreset("hover");s.duration=5;s.maxTime=60;s.parameters["MPC_XY_P"]=1.2;require(h.runner.start({s,s},error),"batch start");h.complete(800);require(h.results.size()==2&&h.results[0].status=="PASS"&&h.results[1].status=="PASS","sequential two-run batch");require(std::abs(h.transport.values["MPC_XY_P"]-.95)<1e-6,"restore controller parameter after each flight");}
    {Harness h;QString error;auto s=labPreset("hover");require(h.runner.start({s},error),"denial setup");h.step();h.successAction();h.step();require(h.transport.pending&&h.transport.next.type=="wind","wind initial async");h.transport.acknowledge(false);h.complete();require(h.results.size()==1&&h.results[0].status=="INCONCLUSIVE","rejected preparation never passes");}
    {Harness h;QString error;auto s=labPreset("hover");s.parameters["MPC_XY_P"]=1.2;require(h.runner.start({s},error),"abort setup");h.step();h.successAction();h.step();h.successAction();h.runner.abort("cancel test");h.complete();require(h.results[0].status=="INCONCLUSIVE"&&std::abs(h.transport.values["MPC_XY_P"]-.95)<1e-6,"cancel rolls back changed params");}
    {Harness h;QString error;auto s=labPreset("hover");require(h.runner.start({s},error),"session loss setup");h.session.active_=false;h.step();require(!h.runner.busy()&&h.results[0].status=="INCONCLUSIVE","session loss terminates batch, cannot pass");}
    {Harness h;QString error;auto s=labPreset("sensor");s.duration=5;s.events={{0,1,4,1,"sensor"}};require(h.runner.start({s},error),"sensor setup");bool denied=false;
        for(int i=0;i<500&&h.runner.busy();++i){if(h.transport.pending){if(h.transport.next.command==420&&h.transport.next.params[1]==1&&!denied){denied=true;h.transport.acknowledge(false);}else h.successAction();}h.step();}
        require(denied&&!h.runner.busy()&&h.results[0].status=="INCONCLUSIVE","unsupported sensor cannot pass");bool cleared=false;for(auto a:h.transport.commands)cleared|=a.command==420&&a.params[0]==4&&a.params[1]==0;require(cleared,"possibly applied sensor failure is cleared even after ACK loss/denial");}
    {auto state=std::make_shared<FlightState>();state->peer="127.0.0.1";TestSession session;QUdpSocket socket;LabTransport t(state,&session,&socket,nullptr);int callbacks=0;QObject::connect(&t,&LabTransport::completed,[&](bool,const QString&,double){++callbacks;});LabAction a;a.type="link";a.a=100;require(t.issue(a),"async link start");t.cancel();a.a=0;require(t.issue(a),"new operation after cancel");QCoreApplication::processEvents();require(callbacks==1,"canceled queued operation cannot complete new operation");}
    std::cout<<"Asynchronous FSM / rollback / ACK denial / batch tests passed\n";
}
