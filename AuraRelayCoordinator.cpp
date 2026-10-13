#include "AuraRelayCoordinator.hpp"
#include "SimulationProtocol.hpp"
#include "FlightReport.hpp"
#include <QUdpSocket>
#include <QFile>
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <limits>
namespace {
LabAction cmd(int command,std::array<float,7> params={}){LabAction a;a.type="command";a.command=command;a.params=params;return a;}
}
AuraRelayCoordinator::AuraRelayCoordinator(std::shared_ptr<FlightState>s,SimulatorSession*session,QUdpSocket*socket,QObject*parent):QObject(parent),source_(std::move(s)),session_(session){
    route_={{12,-8},{28,-8},{28,8},{12,8}};
    for(int n=0;n<3;++n){auto&a=agents_[n];a.state=std::make_shared<FlightState>();a.state->selected=n+1;a.transport=new LabTransport(a.state,session,socket,this);connect(a.transport,&LabTransport::completed,this,[this,n](bool ok,const QString&e,double){complete(n,ok,e);});}
}
void AuraRelayCoordinator::sync(){for(auto&a:agents_){a.state->vehicles=source_->vehicles;a.state->peer=source_->peer;}}
bool AuraRelayCoordinator::live(int n,double now)const{auto v=source_->vehicles.constFind(n+1);return v!=source_->vehicles.cend()&&v->px4&&v->heartbeatAt>0&&now-v->heartbeatAt<3&&v->posAt>0&&now-v->posAt<1&&v->attitudeAt>0&&now-v->attitudeAt<1;}
bool AuraRelayCoordinator::inZone(int n,double now)const{if(!live(n,now))return false;const auto&v=source_->vehicles[n+1];auto point=aero::local(v.lat,v.lon,originLat_,originLon_);return v.armed&&std::abs(v.alt-altitude_)<=1.5&&aero::distance(point,{20,0})<=radius_&&(agents_[n].role==Role::Patrol||agents_[n].role==Role::Transit);}
bool AuraRelayCoordinator::allGround(double now)const{for(int n=0;n<3;++n){auto v=source_->vehicles.constFind(n+1);if(v==source_->vehicles.cend()||v->heartbeatAt<=0||now-v->heartbeatAt>=3||v->landedAt<=0||now-v->landedAt>=3||v->landed!=1||v->armed)return false;}return true;}
void AuraRelayCoordinator::event(const QString&type,int n,const QString&text){events_.append(QJsonObject{{"t",running_?sitl::monoSeconds()-startAt_:0},{"type",type},{"system",n+1},{"text",text}});emit log("Görev İhale Deneyi: "+text);}
void AuraRelayCoordinator::setRole(int n,Role role,double now){auto&a=agents_[n];a.role=role;a.roleAt=now;a.routeSent=false;a.dwellAt=0;a.gate.reset();}
bool AuraRelayCoordinator::request(QString&error){
    if(busy()){error="Görev İhale Deneyi zaten etkin.";return false;}if(!session_->active()){error="Önce üç araçlı IHA Maker oturumu gerekli.";return false;}
    QFile f(":/hardware/scenarios/aura_relay.json");if(!f.open(QIODevice::ReadOnly)){error="Görev İhale Deneyi senaryo kaynağı yok.";return false;}auto o=QJsonDocument::fromJson(f.readAll()).object();
    duration_=o.value("durationS").toDouble(900);trigger_=o.value("handoverSoc").toDouble(.25);chargeA_=o.value("chargeCurrentA").toDouble(1.3);windSpeed_=o.value("windSpeedMS").toDouble(3);
    if(duration_<60||duration_>3600||trigger_<=.1||trigger_>=.8||chargeA_<=0||chargeA_>20||windSpeed_<0||windSpeed_>10){error="Görev İhale Deneyi senaryo sınırları geçersiz.";return false;}
    pending_=true;failed_=false;stopping_=false;requestAt_=sitl::monoSeconds();emit status("Görev İhale Deneyi: üç araç yerde ve hazır bekleniyor…");return true;
}
bool AuraRelayCoordinator::command(int n,const LabAction&a,const QString&context){auto&v=agents_[n];if(v.transport->busy())return false;v.command=context;if(!v.transport->issue(a)){v.command.clear();stop(QString("SYS %1 komutu başlatılamadı: %2").arg(n+1).arg(context));return false;}return true;}
void AuraRelayCoordinator::complete(int n,bool ok,const QString&e){auto&a=agents_[n];const auto context=a.command;a.command.clear();event("command",n,context+": "+e);if(!ok){if(stopping_){failed_=true;failure_+=" "+context+": "+e;if(context=="wind-reset")windRestored_=true;else setRole(n,Role::Fault,sitl::monoSeconds());return;}stop(context+": "+e);return;}
    const double now=sitl::monoSeconds();if(context=="arm")setRole(n,Role::Takeoff,now);else if(context=="takeoff")setRole(n,Role::Transit,now);else if(context=="goto")a.routeSent=true;else if(context=="rtl")setRole(n,Role::Return,now);else if(context=="land")setRole(n,Role::Landing,now);else if(context=="wind-reset")windRestored_=true;
}
void AuraRelayCoordinator::launch(double now){
    sync();startAt_=lastTick_=now;lastUi_=0;running_=true;pending_=false;coverageStarted_=false;windSent_=windRestored_=false;active_=0;replacement_=-1;handovers_=0;coverage_={};events_={};failure_.clear();originLat_=source_->vehicles[1].lat;originLon_=source_->vehicles[1].lon;
    for(int n=0;n<3;++n){auto&a=agents_[n];a.transport->cancel();a.command.clear();a.grace={};a.coveredEnergy=true;a.battery={};auto h=session_->vehicleProfile(n+1).value("hardware").toObject();auto e=h.value("reportEstimates").toObject(),energy=h.value("energyEstimate").toObject();a.battery.capacityMah=e.value("capacityMah").toDouble(1300);a.battery.hoverA=energy.value("hoverCurrentA").toDouble(e.value("hoverCurrentA").toDouble(9.8));a.battery.voltageV=energy.value("voltageV").toDouble(e.value("loadedVoltageV").toDouble(21.9));a.battery.chargeA=chargeA_;a.battery.soc=n==2?.95:1;
        a.result={};a.result.id=QUuid::createUuid().toString(QUuid::WithoutBraces);a.result.started=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);a.result.spec=labPreset("route");a.result.spec.name=QString("Görev İhale Deneyi SYS %1").arg(n+1);a.result.spec.vehicle=QString("iha_maker_%1").arg(n+1);a.result.spec.versionTag="Görev İhale Deneyi";a.result.spec.duration=duration_;a.result.spec.altitude=altitude_;a.result.spec.route=route_;a.result.spec.batteryModel="Görev İhale Deneyi live hover/tilt estimate; PX4 battery independent";a.point=0;a.result.spec.maxTime=duration_+180;a.result.profileHash=session_->profileHashFor(n+1);a.result.firmware=session_->firmware();a.result.originLat=originLat_;a.result.originLon=originLon_;a.result.metrics["systemId"]=n+1;a.result.metrics["ihaHardware"]=h;setRole(n,n==2?Role::Charging:Role::Standby,now);
    }
    setRole(0,Role::Arm,now);event("start",0,"Drone 1 aktif başlangıç; Drone 2 yedek %100; Drone 3 şarjda %95 (model başlangıcı).");emit status("Görev İhale Deneyi başladı; akım/SOC/şarj değerleri tahmini modeldir.");
}
void AuraRelayCoordinator::step(int n,double now){
    auto&a=agents_[n];if(!live(n,now))return;const auto&v=source_->vehicles[n+1];auto p=aero::local(v.lat,v.lon,originLat_,originLon_);
    if(a.role==Role::Charging){if(a.battery.soc>=.999)setRole(n,Role::Standby,now);return;}if(a.role==Role::Standby||a.role==Role::Fault||a.transport->busy())return;
    if(a.role==Role::Arm){if(v.armed)setRole(n,Role::Takeoff,now);else command(n,cmd(400,{1,0,0,0,0,0,0}),"arm");return;}
    if(a.role==Role::Takeoff){if(!v.armed)return;float nan=std::numeric_limits<float>::quiet_NaN();command(n,cmd(22,{0,0,0,nan,nan,nan,float(v.amsl-v.alt+altitude_)}),"takeoff");return;}
    if(a.role==Role::Transit){if(std::abs(v.alt-altitude_)>1.5){if(now-a.roleAt>60)stop("Kalkış irtifası 60 saniyede doğrulanmadı.");return;}
        if(!a.routeSent){auto geo=aero::geographic({20,0},originLat_,originLon_);LabAction action;action.type="goto";action.a=geo.north;action.b=geo.east;action.c=v.amsl-v.alt+altitude_;action.d=3;command(n,action,"goto");return;}
        bool arrived=live(n,now)&&v.armed&&aero::distance(p,{20,0})<=3&&std::abs(v.alt-altitude_)<=1.5;
        if(a.gate.update(now,arrived)){if(n==active_&&replacement_<0){setRole(n,Role::Patrol,now);if(!coverageStarted_){coverageStarted_=true;coverageAt_=now;}event("patrol",n,"Devriye alanına ulaşma doğrulandı.");}else if(n==replacement_){int old=active_;active_=n;replacement_=-1;++handovers_;setRole(n,Role::Patrol,now);setRole(old,Role::Return,now);event("handover",n,QString("SYS %1 devraldı; SYS %2 ancak şimdi RTL'ye ayrılıyor.").arg(n+1).arg(old+1));}}
        else if(now-a.roleAt>90)stop("Yedek devriye noktasına 90 saniyede ulaşamadı.");return;}
    if(a.role==Role::Patrol){auto target=route_[a.point%route_.size()];if(!a.routeSent){auto geo=aero::geographic(target,originLat_,originLon_);LabAction action;action.type="goto";action.a=geo.north;action.b=geo.east;action.c=v.amsl-v.alt+altitude_;action.d=3;command(n,action,"goto");return;}if(aero::distance(p,target)<=2){if(!a.dwellAt)a.dwellAt=now;if(now-a.dwellAt>=1){a.point=(a.point+1)%route_.size();a.routeSent=false;a.dwellAt=0;}}return;}
    if(a.role==Role::Return){if(v.landed==1&&now-v.landedAt<3){setRole(n,v.armed?Role::Disarm:Role::Charging,now);return;}
        int main=(v.customMode>>16)&255,sub=(v.customMode>>24)&255;if(main!=4||(sub!=5&&sub!=6)){if(now-a.roleAt>120){stop("RTL dönüşü 120 saniyede doğrulanmadı.");return;}command(n,cmd(20),"rtl");}else if(now-a.roleAt>120)stop("RTL inişi 120 saniyede doğrulanmadı.");return;}
    if(a.role==Role::Landing){if(v.landed==1&&now-v.landedAt<3)setRole(n,v.armed?Role::Disarm:Role::Charging,now);else if(now-a.roleAt>120){failed_=true;failure_+=" İniş doğrulanamadı.";setRole(n,Role::Fault,now);}return;}
    if(a.role==Role::Disarm){if(!v.armed){setRole(n,Role::Charging,now);return;}command(n,cmd(400,{0,0,0,0,0,0,0}),"disarm");}
}
void AuraRelayCoordinator::tick(double now){
    sync();if(pending_){bool ready=session_->owned()&&session_->running()&&session_->vehicleCount()==3&&allGround(now);for(int n=0;n<3;++n)ready&=live(n,now)&&!session_->profileHashFor(n+1).isEmpty()&&session_->vehicleProfile(n+1).contains("hardware");if(ready)launch(now);else if(now-requestAt_>240){pending_=false;emit status("Görev İhale Deneyi: araçlar 240 saniyede hazır olmadı.");}return;}
    if(!running_)return;double dt=now-lastTick_;lastTick_=now;if(dt>1||dt<=0){dt=0;failed_=true;failure_+=" İzleme zaman boşluğu.";}
    for(int n=0;n<3;++n){auto&a=agents_[n];a.transport->tick(now);if(live(n,now)){a.grace.expired(now,true);const auto&v=source_->vehicles[n+1];bool air=v.armed&&v.alt>.3;a.coveredEnergy&=a.battery.update(dt,air,a.role==Role::Charging&&!v.armed&&v.landed==1,v.roll,v.pitch);auto p=aero::local(v.lat,v.lon,originLat_,originLon_);if(a.result.samples.size()<40000)a.result.samples.append({now-startAt_,v.bootMs/1000.,p.east,p.north,v.alt,aero::distance(p,{20,0}),std::abs(v.alt-altitude_),v.roll,v.pitch,v.yaw,int(v.customMode),int(a.battery.soc*100),true});}else{a.coveredEnergy=false;if(a.grace.expired(now,false)&&source_->vehicles.value(n+1).armed&&!stopping_)stop(QString("SYS %1 uçuş telemetrisi 3 saniyede toparlanmadı.").arg(n+1));}}
    if(coverageStarted_&&!stopping_){int observers=0;for(int n=0;n<3;++n)observers+=inZone(n,now)?1:0;coverage_.update(dt,observers);}
    if(!stopping_){if(now-startAt_>=duration_)stop({});else {
        if(!windSent_&&now-startAt_>=windAt_&&!agents_[0].transport->busy()){windSent_=true;LabAction a;a.type="wind";a.a=windSpeed_;a.b=90;command(0,a,"wind");}
        if(agents_[active_].role==Role::Patrol&&replacement_<0&&aura::needsRelief(agents_[active_].battery.soc,trigger_)){for(int offset=1;offset<=2;++offset){int candidate=(active_+offset)%3;if(agents_[candidate].role==Role::Standby&&agents_[candidate].battery.soc>=.99){replacement_=candidate;setRole(candidate,Role::Arm,now);event("relief-request",candidate,"Tahmini SOC eşik altında; yedek kalkıyor, aktif araç bölgede kalıyor.");break;}}}
        if(agents_[active_].battery.soc<=critical_){stop("Tahmini SOC kritik; kesintisizlik için batarya sınırı aşılmaz.");}
    }}
    for(int n=0;n<3;++n)step(n,now);
    if(stopping_&&allGround(now)&&!agents_[0].transport->busy()&&!windRestored_){LabAction reset;reset.type="wind";reset.a=0;reset.b=0;command(0,reset,"wind-reset");}
    if(stopping_&&allGround(now)&&windRestored_)finalise();
    if(running_&&stopping_&&now-stopAt_>150){failed_=true;failure_+=" İniş/rüzgar temizliği 150 saniyede doğrulanmadı; operatör canlı durumu kontrol etmeli.";finalise();}
    if(now-lastUi_>.25){lastUi_=now;QJsonArray rows;for(int n=0;n<3;++n){const auto&a=agents_[n];QString role=a.role==Role::Patrol?"AKTİF":a.role==Role::Standby?"YEDEK":a.role==Role::Charging?"ŞARJ":a.role==Role::Return?"RTL":a.role==Role::Transit?"DEVİR NOKTASINA":a.role==Role::Fault?"HATA":"GEÇİŞ / İNİŞ";rows.append(QJsonObject{{"system",n+1},{"role",role},{"socPct",a.battery.soc*100},{"currentA",a.battery.currentA},{"voltageV",a.battery.voltageV},{"timeS",now-startAt_},{"estimated",true}});}emit telemetry(rows);}
}
void AuraRelayCoordinator::feed(const sitl::TelemetryPacketData&p){if(!running_)return;for(auto&a:agents_)if(p.system==a.state->selected)a.transport->feed(p);}
void AuraRelayCoordinator::stop(const QString&reason){if(!reason.isEmpty()){failed_=true;failure_+=reason;}if(stopping_)return;stopping_=true;stopAt_=sitl::monoSeconds();event("stop",active_,reason.isEmpty()?"Test süresi bitti; tüm araçlar inişe yönlendiriliyor.":reason);for(int n=0;n<3;++n){auto&a=agents_[n];a.transport->cancel();a.command.clear();const auto v=source_->vehicles.value(n+1);if(v.armed){setRole(n,Role::Landing,sitl::monoSeconds());float nan=std::numeric_limits<float>::quiet_NaN();command(n,cmd(21,{0,0,0,nan,nan,nan,nan}),"land");}}}
void AuraRelayCoordinator::abort(){pending_=false;if(running_)stop("Operatör Görev İhale Deneyi testini iptal etti.");else emit status("Görev İhale Deneyi başlatma iptal edildi.");}
void AuraRelayCoordinator::sessionEnded(){pending_=false;if(running_){failed_=true;failure_+=" Simülasyon kapandı; iniş doğrulanmadı.";finalise();}}
void AuraRelayCoordinator::finalise(){
    running_=false;pending_=false;for(int n=0;n<3;++n){auto&a=agents_[n];a.transport->cancel();auto&r=a.result;r.status=(failed_||!coverageStarted_||coverage_.durationS<=0||handovers_==0||!a.coveredEnergy)?"INCONCLUSIVE":coverage_.gapS>0?"FAIL":"PASS";r.reason=failed_?failure_:handovers_==0?"Nöbet değişimi gerçekleşmedi; röle görevi doğrulanamadı.":!a.coveredEnergy?"Tahmini enerji hesabında telemetri kapsamı eksik.":"Konumsal görev mevcudiyeti hesaplandı; gerçek kamera gözlemi doğrulanmadı.";r.eventLog=events_;r.metrics["durationS"]=sitl::monoSeconds()-startAt_;r.metrics["measurementS"]=coverage_.durationS;r.metrics["relayMission"]=QJsonObject{{"coveragePct",coverage_.percent()},{"windowStartsAfterInitialArrival",true},{"startupBeforeCoverageS",coverageStarted_?coverageAt_-startAt_:0},{"coveredS",coverage_.coveredS},{"gapS",coverage_.gapS},{"longestGapS",coverage_.longestGapS},{"overlapS",coverage_.overlapS},{"handovers",handovers_},{"observability","Fresh telemetry + in-zone position/altitude; NOT camera observation"},{"energySource","Live hover-current/tilt estimate; NOT MAVLink measured battery"},{"energyCoverageComplete",a.coveredEnergy},{"usedMah",a.battery.usedMah},{"chargedMah",a.battery.chargedMah},{"finalSocPct",a.battery.soc*100}};emit finished(r);}emit status(failed_?"Görev İhale Deneyi doğrulanamadı; kayıtları ve canlı iniş durumunu kontrol et.":"Görev İhale Deneyi tamamlandı; kesintisizlik sonuçlardan hesaplandı.");
}
