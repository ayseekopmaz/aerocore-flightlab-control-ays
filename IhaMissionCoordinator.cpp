#include "IhaMissionCoordinator.hpp"
#include "FleetPanel.hpp"
#include "SimulationProtocol.hpp"
#include <QUdpSocket>
IhaMissionCoordinator::IhaMissionCoordinator(std::shared_ptr<FlightState>s,SimulatorSession*session,QUdpSocket*socket,QObject*parent):QObject(parent),source_(std::move(s)),session_(session){
    for(int n=0;n<3;++n){auto &a=agents_[n];a.state=std::make_shared<FlightState>();a.state->selected=n+1;a.transport=new LabTransport(a.state,session,socket,this);a.runner=new ExperimentRunner(a.state,session,a.transport,this);
        a.runner->measurementBarrier=[this]{if(barrierOpen_)return sitl::monoSeconds()>=barrierAt_;for(const auto &a:agents_)if(!a.runner->settled())return false;barrierOpen_=true;barrierAt_=sitl::monoSeconds()+.2;emit log("IHA Maker: üç araç kararlı; ortak ölçüm ve rota başlangıcı.");return false;};
        a.runner->commonMeasurementEpoch=[this]{return barrierAt_;};
        connect(a.runner,&ExperimentRunner::log,this,[this,n](const QString&line){emit log(QString("IHA SYS %1: ").arg(n+1)+line);});
        connect(a.runner,&ExperimentRunner::finished,this,[this,n](const LabResult&input){auto r=input;r.metrics["concurrentMission"]=true;if(barrierOpen_)r.metrics["commonMeasurementHostTimeS"]=barrierAt_;emit finished(r);if(r.status!="PASS"&&!cancelling_){cancelling_=true;for(int j=0;j<3;++j)if(j!=n&&agents_[j].runner->busy())agents_[j].runner->abort("Eş zamanlı görevde bir araç testi tamamlayamadı; iniş.");}});
    }
}
void IhaMissionCoordinator::sync(){for(auto &a:agents_){a.state->vehicles=source_->vehicles;a.state->peer=source_->peer;}}
bool IhaMissionCoordinator::request(QString&error){if(busy()){error="IHA Maker görevi zaten etkin.";return false;}if(!session_->active()){error="Önce üç araçlı simülasyon başlatılmalı.";return false;}pending_=true;cancelling_=false;barrierOpen_=false;requestedAt_=sitl::monoSeconds();emit status("Üç aracın PX4 ve yerde telemetrisi bekleniyor…");return true;}
bool IhaMissionCoordinator::ready(double now)const{
    if(!session_->owned()||!session_->running()||session_->vehicleCount()!=3||source_->peer.isEmpty())return false;
    for(int n=1;n<=3;++n){auto v=source_->vehicles.constFind(n);if(v==source_->vehicles.cend()||!v->px4||v->armed||v->landed!=1||v->posAt<=0||now-v->posAt>=1||v->heartbeatAt<=0||now-v->heartbeatAt>=3||v->landedAt<=0||now-v->landedAt>=3||session_->profileHashFor(n).isEmpty()||!session_->vehicleProfile(n).contains("hardware"))return false;}
    return true;
}
bool IhaMissionCoordinator::launch(QString&error){
    sync();std::array<LabSpec,3> specs;
    for(int n=0;n<3;++n){auto &s=specs[n];s=labPreset("route");s.name=QString("IHA Maker eş zamanlı rota SYS %1").arg(n+1);s.versionTag="IHA_three_routes";s.vehicle=QString("iha_maker_%1").arg(n+1);s.duration=60;s.maxTime=240;s.altitude=5+2*n;s.speed=3;
        s.route=n==0?QVector<aero::Point>{{12,0},{12,8},{0,8},{0,0}}:n==1?QVector<aero::Point>{{0,12},{8,12},{8,0},{0,0}}:QVector<aero::Point>{{-12,0},{-12,-8},{0,-8},{0,0}};
        auto v=VehicleProfile::fromJson(session_->vehicleProfile(n+1),error);if(!v)return false;v->apply(s);error=s.validate();if(!error.isEmpty())return false;
    }
    running_=true;pending_=false;barrierOpen_=false;
    for(int n=0;n<3;++n)if(!agents_[n].runner->start({specs[n]},error)){abort();return false;}
    emit status("Üç görev birlikte başlatıldı: doğu / kuzey / batı");return true;
}
void IhaMissionCoordinator::tick(double now){
    sync();if(pending_){if(ready(now)){QString error;if(!launch(error)){pending_=false;emit log(error);emit status("Başlatılamadı: "+error);}}else if(now-requestedAt_>240){pending_=false;emit status("Üç araç 240 saniyede hazır olmadı; olay kaydını kontrol et.");}}
    if(running_){for(auto &a:agents_){a.transport->tick(now);a.runner->tick(now);}bool any=false;for(const auto&a:agents_)any|=a.runner->busy();if(!any){running_=false;emit status(cancelling_?"Görev iptal edildi; sonuçları kontrol et.":"Üç görev tamamlandı; sonuçları kontrol et.");}}
}
void IhaMissionCoordinator::feed(const sitl::TelemetryPacketData&p){if(!running_)return;for(auto &a:agents_)if(p.system==a.state->selected)a.transport->feed(p);}
void IhaMissionCoordinator::abort(){pending_=false;cancelling_=true;for(auto &a:agents_)if(a.runner->busy())a.runner->abort("Operatör IHA Maker görevini iptal etti; iniş.");emit status("Görev iptal / iniş işlemleri sürüyor…");}
void IhaMissionCoordinator::sessionEnded(){pending_=false;sync();if(running_)for(auto&a:agents_)a.runner->tick(sitl::monoSeconds());running_=false;emit status("IHA Maker oturumu kapalı.");}
