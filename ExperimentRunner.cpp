#include "ExperimentRunner.hpp"
#include <QDateTime>
#include <QUuid>
#include <QJsonDocument>
#include <limits>
#include <cmath>
namespace {
LabAction cmd(int c,std::array<float,7> p){LabAction a;a.type="command";a.command=c;a.params=p;return a;}
QString modeName(quint32 mode){int main=(mode>>16)&255,sub=(mode>>24)&255;if(main==4){if(sub==5)return "Return";if(sub==6)return "Land";if(sub==3)return "Hold";}return QString::number(mode);}
}
ExperimentRunner::ExperimentRunner(std::shared_ptr<FlightState>s,SimulatorSession*session,LabTransport*t,QObject*p):QObject(p),state_(std::move(s)),session_(session),transport_(t){connect(t,&LabTransport::completed,this,&ExperimentRunner::done);}
QString ExperimentRunner::phaseText()const{switch(phase_){case Phase::Idle:return "Hazır";case Phase::Prepare:return "Koşullar hazırlanıyor";case Phase::Arm:return "ARM onayı";case Phase::Takeoff:return "Kalkış";case Phase::Settle:return "Kararlılık bekleniyor";case Phase::Measure:return "Ölçüm / olaylar";case Phase::Cleanup:return "Olaylar temizleniyor";case Phase::Land:return "İniş doğrulanıyor";case Phase::Disarm:return "DISARM doğrulanıyor";case Phase::Restore:return "Koşullar geri yükleniyor";case Phase::Between:return "Sıradaki deney";}return {};}
bool ExperimentRunner::start(const QVector<LabSpec>&specs,QString&error){
    if(busy()||transport_->busy()){error="Bir test / eylem zaten çalışıyor.";return false;}if(specs.isEmpty()||specs.size()>100){error="Deney sayısı 1–100 olmalı.";return false;}
    const double now=sitl::monoSeconds();auto v=state_->vehicles.constFind(state_->selected);
    if(!session_->owned()||!session_->running()||session_->profileHash().isEmpty()||v==state_->vehicles.cend()||!v->px4||v->armed||v->landed!=1||now-v->heartbeatAt>=3||now-v->landedAt>=3||now-v->posAt>=1){error="Panelin başlattığı canlı x500, taze konum ve DISARMED / YERDE durumu gerekli.";return false;}
    for(auto s:specs){error=s.validate();if(!error.isEmpty())return false;if(s.profile()!=session_->labProfile()){error="Yük / GPS model profili farklı. Senaryoyu seçip yerde simülasyonu kapatın ve yeniden başlatın.";return false;}}
    specs_=specs;runIndex_=0;stopping_=false;begin(now);return true;
}
void ExperimentRunner::setPhase(Phase p,double now){phase_=p;phaseAt_=now;emit changed();}
void ExperimentRunner::note(const QString&type,const QString&text,bool ok){result_.eventLog.append(QJsonObject{{"t",sitl::monoSeconds()-startAt_},{"type",type},{"ok",ok},{"text",text}});emit log(text);}
void ExperimentRunner::begin(double now){
    const auto&v=state_->vehicles[state_->selected];result_=LabResult{};result_.spec=specs_[runIndex_];result_.id=QUuid::createUuid().toString(QUuid::WithoutBraces);result_.started=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);result_.profileHash=session_->profileHash();result_.firmware=session_->firmware();result_.originLat=v.lat;result_.originLon=v.lon;
    result_.metrics["systemId"]=state_->selected;result_.metrics["vehicleName"]=session_->vehicleProfile(state_->selected)["name"].toString();
    startAt_=lastTick_=now;measureAt_=lastPosition_=lastPower_=lastActuators_=withinAt_=dwellAt_=landAt_=climbAt_=0;freshSeconds_=0;point_=event_=0;parameter_=0;actuatorRequested_=activeEvent_=eventEnding_=routeSent_=complete_=unsafe_=sensorObserved_=modeObserved_=climbReached_=false;waiting_.clear();fatal_.clear();originals_.clear();horizontal_={};vertical_={};tilt_={};energy_={};initialMode_=int(v.customMode);sensorNeedsReset_=false;
    desired_=result_.spec.parameters;
    for(auto e:result_.spec.events)if(e.type=="sensor")desired_["SYS_FAILURE_EN"]=1;
    if(result_.spec.kind=="battery"||result_.spec.batteryDrain>0){desired_["SIM_BAT_DRAIN"]=result_.spec.batteryDrain;desired_["SIM_BAT_MIN_PCT"]=result_.spec.batteryMin;}
    keys_=desired_.keys();transport_->resetFault();setPhase(Phase::Prepare,now);note("start",QString("Deney %1/%2 — %3; sürüm etiketi %4").arg(runIndex_+1).arg(specs_.size()).arg(result_.spec.name,result_.spec.versionTag),true);
}
bool ExperimentRunner::action(const LabAction&a,const QString&context){if(!waiting_.isEmpty())return false;waiting_=context;pendingAction_=a;if(!transport_->issue(a)){QTimer::singleShot(0,this,[this]{done(false,"Eylem başlatılamadı / taşıyıcı meşgul.",0);});return false;}return true;}
void ExperimentRunner::done(bool ok,const QString&evidence,double value){
    if(!busy()||waiting_.isEmpty())return;const auto context=waiting_;const auto a=pendingAction_;waiting_.clear();note(context,evidence,ok);
    if(context=="actuator-probe")return; // Optional stream: rejection is recorded, never fabricated.
    if(!ok){if(phase_==Phase::Cleanup){unsafe_=true;fatal_+=" Olay temizliği doğrulanamadı: "+context;if(context=="cleanup-event"){sensorNeedsReset_=false;activeEvent_=false;sensorNeedsReset_=false;}else setPhase(Phase::Land,sitl::monoSeconds());return;}if(phase_==Phase::Restore){unsafe_=true;stopping_=true;fatal_+=" Geri yükleme başarısız: "+context;++parameter_;return;}if(phase_==Phase::Land||phase_==Phase::Disarm){fatal_+=" İniş/DISARM komutu doğrulanamadı.";unsafe_=true;stopping_=true;setPhase(Phase::Restore,sitl::monoSeconds());keys_=originals_.keys();parameter_=0;return;}abort(context+": "+evidence);return;}
    if(context=="param-read"){originals_[a.name]=value;return;}
    if(context=="param-set"){++parameter_;return;}
    if(context=="param-restore"){++parameter_;return;}
    if(context=="wind-initial"){setPhase(Phase::Arm,sitl::monoSeconds());return;}
    if(context=="arm"){setPhase(Phase::Takeoff,sitl::monoSeconds());return;}
    if(context=="takeoff"){climbAt_=sitl::monoSeconds();setPhase(Phase::Settle,climbAt_);return;}
    if(context=="route"){routeSent_=true;return;}
    if(context=="event-start"){activeEvent_=true;return;}
    if(context=="event-end"){if(a.type=="command"&&a.command==420)sensorNeedsReset_=false;activeEvent_=false;eventEnding_=false;++event_;return;}
    if(context=="cleanup-event"){sensorNeedsReset_=false;activeEvent_=false;eventEnding_=false;return;}
    if(context=="cleanup-wind"){setPhase(Phase::Land,sitl::monoSeconds());return;}
    if(context=="land")return;
    if(context=="disarm"){setPhase(Phase::Disarm,sitl::monoSeconds());return;}
}
void ExperimentRunner::prepare(double now){if(parameter_<keys_.size()){QString k=keys_[parameter_];LabAction a;a.name=k;if(!originals_.contains(k)){a.type="read";action(a,"param-read");}else{a.type="set";a.a=desired_[k];action(a,"param-set");}return;}
    if(!actuatorRequested_){actuatorRequested_=true;action(cmd(511,{375,100000,0,0,0,0,0}),"actuator-probe");return;}
    LabAction a;a.type="wind";a.a=result_.spec.windSpeed;a.b=result_.spec.windDirection;action(a,"wind-initial");Q_UNUSED(now);
}
void ExperimentRunner::abort(const QString&reason){if(!busy())return;stopping_=true;complete_=false;fatal_=reason;note("abort",reason,false);transport_->cancel();waiting_.clear();cleanup(sitl::monoSeconds());}
void ExperimentRunner::cleanup(double now){transport_->resetFault();setPhase(Phase::Cleanup,now);}
void ExperimentRunner::land(double now){auto it=state_->vehicles.constFind(state_->selected);bool ground=it!=state_->vehicles.cend()&&it->landedAt>0&&now-it->landedAt<3&&it->landed==1;
    if(ground){if(!landAt_)landAt_=now;if(it->armed){setPhase(Phase::Disarm,now);action(cmd(400,{0,0,0,0,0,0,0}),"disarm");}else{keys_=originals_.keys();parameter_=0;setPhase(Phase::Restore,now);}return;}
    if(!landAt_){const float nan=std::numeric_limits<float>::quiet_NaN();landAt_=-now;action(cmd(21,{0,0,0,nan,nan,nan,nan}),"land");return;}
    if(now-phaseAt_>90){unsafe_=true;stopping_=true;fatal_+=" İniş 90 sn içinde doğrulanamadı. Panelden uçuş durumunu kontrol edin.";keys_=originals_.keys();parameter_=0;setPhase(Phase::Restore,now);}
}
void ExperimentRunner::tick(double now){if(!busy())return;
    if(!session_->running()){transport_->cancel();waiting_.clear();unsafe_=true;stopping_=true;fatal_="Simülasyon oturumu sona erdi; koşul geri yükleme doğrulanamadı.";finalise(now);return;}
    auto it=state_->vehicles.constFind(state_->selected);if(it==state_->vehicles.cend()){transport_->cancel();waiting_.clear();unsafe_=true;stopping_=true;fatal_="Seçili araç kayboldu; iniş ve geri yükleme doğrulanamadı.";finalise(now);return;}const auto &v=*it;
    const bool pos=v.posAt>0&&now-v.posAt<1,att=v.attitudeAt>0&&now-v.attitudeAt<1,live=v.heartbeatAt>0&&now-v.heartbeatAt<3;
    double dt=std::clamp(now-lastTick_,0.,.25);lastTick_=now;
    if(phase_==Phase::Measure){double t=now-measureAt_;bool fresh=pos&&att&&live;if(fresh)freshSeconds_+=dt;
        if(v.posAt>lastPosition_&&pos){lastPosition_=v.posAt;auto p=aero::local(v.lat,v.lon,result_.originLat,result_.originLon);aero::Point a{},b{};if(!result_.spec.route.isEmpty()){int index=std::min(point_,int(result_.spec.route.size())-1);b=result_.spec.route[index];if(index>0)a=result_.spec.route[index-1];}
            double error=result_.spec.route.isEmpty()?aero::distance(p):aero::segmentError(p,a,b);double ae=std::abs(v.alt-result_.spec.altitude);
            if(fresh){horizontal_.add(error);vertical_.add(ae);tilt_.add(std::max(std::abs(v.roll),std::abs(v.pitch)));}
            if(result_.samples.size()<40000)result_.samples.append({t,v.bootMs/1000.,p.east,p.north,v.alt,error,ae,v.roll,v.pitch,v.yaw,int(v.customMode),v.battery,fresh});
        }
        if(v.actuatorsAt>lastActuators_&&now-v.actuatorsAt<1){lastActuators_=v.actuatorsAt;double peak=-1e100,low=1e100;int count=0;
            for(int i=0;i<4;++i)if(v.activeOutputs&(1u<<i)){double output=v.actuators[i];if(std::isfinite(output)){peak=std::max(peak,output);low=std::min(low,output);++count;}}
            if(count){result_.metrics["actuatorPeakRaw"]=std::max(result_.metrics.contains("actuatorPeakRaw")?result_.metrics["actuatorPeakRaw"].toDouble():peak,peak);result_.metrics["actuatorSpreadRaw"]=std::max(result_.metrics["actuatorSpreadRaw"].toDouble(),peak-low);}
        }
        if(v.powerAt>lastPower_){lastPower_=v.powerAt;std::optional<double> watts;if(v.powerValid&&now-v.powerAt<2.5)watts=v.voltage*v.current;energy_.add(v.powerAt,watts);}
        if(live&&!result_.spec.expectedMode.isEmpty()&&modeName(v.customMode)==result_.spec.expectedMode){if(!modeObserved_)result_.metrics["modeResponseS"]=t;modeObserved_=true;}
        if(activeEvent_&&event_<result_.spec.events.size()&&result_.spec.events[event_].type=="sensor"){
            auto e=result_.spec.events[event_];if((e.a==4&&v.gpsAt>0&&now-v.gpsAt<3&&v.fixType<3)||int(v.customMode)!=initialMode_){if(!sensorObserved_)result_.metrics["sensorResponseS"]=std::max(0.,t-e.at);sensorObserved_=true;}
        }
        if(activeEvent_&&event_<result_.spec.events.size()&&result_.spec.events[event_].type=="link"&&!live&&!result_.metrics.contains("linkWarningS"))result_.metrics["linkWarningS"]=std::max(0.,t-result_.spec.events[event_].at);
        if(!activeEvent_&&event_>0&&result_.spec.events[event_-1].type=="wind"&&fresh&&!result_.metrics.contains("windRecoveryS")){
            auto e=result_.spec.events[event_-1];auto p=aero::local(v.lat,v.lon,result_.originLat,result_.originLon);
            if(result_.spec.route.isEmpty()&&aero::distance(p)<=result_.spec.maxError&&std::abs(v.alt-result_.spec.altitude)<=result_.spec.maxAltError)result_.metrics["windRecoveryS"]=std::max(0.,t-e.at-e.duration);
        }
        if(!activeEvent_&&event_>0&&result_.spec.events[event_-1].type=="link"&&live&&!result_.metrics.contains("linkRecoveryS"))result_.metrics["linkRecoveryS"]=std::max(0.,t-result_.spec.events[event_-1].at-result_.spec.events[event_-1].duration);
    }
    if(!waiting_.isEmpty())return;
    if(phase_==Phase::Cleanup){
        if(sensorNeedsReset_){
            action(cmd(420,{float(lastSensorUnit_),0,0,0,0,0,0}),"cleanup-event");return;
        }
        LabAction a;a.type="wind";a.a=0;a.b=0;action(a,"cleanup-wind");return;
    }
    if(phase_==Phase::Restore){if(parameter_<keys_.size()){LabAction a;a.type="set";a.name=keys_[parameter_];a.a=originals_[a.name];action(a,"param-restore");return;}finalise(now);return;}
    if(phase_==Phase::Land){land(now);return;}
    if(phase_==Phase::Disarm){if(!v.armed&&v.landed==1&&now-v.landedAt<3){keys_=originals_.keys();parameter_=0;setPhase(Phase::Restore,now);}else if(now-phaseAt_>10){unsafe_=true;stopping_=true;fatal_+=" DISARM durumu doğrulanamadı.";keys_=originals_.keys();parameter_=0;setPhase(Phase::Restore,now);}return;}
    if(phase_==Phase::Between){if(now-phaseAt_>2){if(!live||v.armed||v.landed!=1||now-v.landedAt>=3||!pos){stopping_=true;phase_=Phase::Idle;emit log("Toplu deney durdu: yeni uçuş için taze yerde/DISARMED bilgisi yok.");emit batchFinished();return;}begin(now);}return;}
    if(!fatal_.isEmpty()||complete_){cleanup(now);return;}
    if(now-startAt_>result_.spec.maxTime){abort("Toplam deney zaman aşımı.");return;}
    bool intentionalGap=activeEvent_&&event_<result_.spec.events.size()&&result_.spec.events[event_].type=="link";
    bool recovering=false;
    if(measureAt_>0&&event_>0&&result_.spec.events[event_-1].type=="link"){auto e=result_.spec.events[event_-1];recovering=now-measureAt_<e.at+e.duration+4;}
    if(!live&&!intentionalGap&&!recovering){abort("Beklenmeyen telemetri kopması.");return;}
    if(phase_==Phase::Prepare){prepare(now);return;}
    if(phase_==Phase::Arm){if(!v.armed)action(cmd(400,{1,0,0,0,0,0,0}),"arm");else setPhase(Phase::Takeoff,now);return;}
    if(phase_==Phase::Takeoff){if(!v.armed){if(now-phaseAt_>8)abort("ARM kabul edildi ancak ARMED durumu doğrulanmadı.");return;}if(!pos){abort("Kalkış için taze konum bilgisi yok.");return;}float nan=std::numeric_limits<float>::quiet_NaN();action(cmd(22,{0,0,0,nan,nan,nan,float(v.amsl-v.alt+result_.spec.altitude)}),"takeoff");return;}
    if(phase_==Phase::Settle){if(pos){auto p=aero::local(v.lat,v.lon,result_.originLat,result_.originLon);result_.metrics["takeoffOvershootM"]=std::max(result_.metrics["takeoffOvershootM"].toDouble(),std::max(0.,v.alt-result_.spec.altitude));if(!climbReached_&&v.alt>=result_.spec.altitude-.5){climbReached_=true;result_.metrics["riseTimeS"]=now-climbAt_;}
        if(std::abs(v.alt-result_.spec.altitude)<=.5&&aero::distance(p)<=result_.spec.maxError){if(!withinAt_)withinAt_=now;if(now-withinAt_>=2){measureAt_=now;result_.metrics["measurementStartS"]=now-startAt_;initialMode_=int(v.customMode);setPhase(Phase::Measure,now);}}else withinAt_=0;}
        if(now-phaseAt_>60)abort("Kalkış / kararlı hover 60 sn içinde doğrulanamadı.");return;}
    if(phase_==Phase::Measure){double t=now-measureAt_;
        if(event_<result_.spec.events.size()){auto e=result_.spec.events[event_];if((!activeEvent_&&t>=e.at)||(activeEvent_&&t>=e.at+e.duration)){
            bool ending=activeEvent_;LabAction a;a.type=e.type;
            if(e.type=="sensor"){if(!ending){sensorNeedsReset_=true;lastSensorUnit_=int(e.a);}a=cmd(420,{float(e.a),ending?0.f:float(e.b),0,0,0,0,0});}
            else {a.a=ending?(e.type=="wind"?result_.spec.windSpeed:0):e.a;a.b=ending?(e.type=="wind"?result_.spec.windDirection:0):e.b;}
            eventEnding_=ending;action(a,ending?"event-end":"event-start");return;}}
        if(!result_.spec.route.isEmpty()&&point_<result_.spec.route.size()&&!intentionalGap&&live&&pos){auto target=result_.spec.route[point_];
            if(!routeSent_){auto geo=aero::geographic(target,result_.originLat,result_.originLon);LabAction a;a.type="goto";a.a=geo.north;a.b=geo.east;a.c=v.amsl-v.alt+result_.spec.altitude;a.d=result_.spec.speed;action(a,"route");return;}
            auto p=aero::local(v.lat,v.lon,result_.originLat,result_.originLon);if(aero::distance(p,target)<=.75&&std::abs(v.alt-result_.spec.altitude)<=1){if(!dwellAt_)dwellAt_=now;if(now-dwellAt_>=result_.spec.dwell){++point_;routeSent_=false;dwellAt_=0;note("waypoint",QString("Rota noktası %1 doğrulandı.").arg(point_),true);}}else dwellAt_=0;
        }
        bool routeDone=result_.spec.route.isEmpty()||point_>=result_.spec.route.size();if(t>=result_.spec.duration&&routeDone&&event_>=result_.spec.events.size()&&!activeEvent_){complete_=true;result_.metrics["measurementS"]=t;cleanup(now);}
    }
}
void ExperimentRunner::finalise(double now){
    if(phase_==Phase::Idle)return;const double measurement=result_.metrics.contains("measurementS")?result_.metrics["measurementS"].toDouble():measureAt_>0?std::max(0.,now-measureAt_):0;
    double coverage=measurement>0?std::clamp(freshSeconds_/measurement,0.,1.):0;
    QJsonObject original;for(auto it=originals_.begin();it!=originals_.end();++it)original[it.key()]=it.value();result_.metrics["originalParameters"]=original;
    result_.metrics["coverage"]=coverage;result_.metrics["durationS"]=now-startAt_;result_.metrics["samples"]=int(horizontal_.count);result_.metrics["waypointsReached"]=point_;result_.metrics["energyCoveredS"]=energy_.covered;
    if(result_.samples.size()>1){auto first=result_.samples.first(),last=result_.samples.last();double dt=last.t-first.t;if(dt>0&&last.boot>=first.boot)result_.metrics["simToHostTimeRatio"]=(last.boot-first.boot)/dt;}
    if(horizontal_.count){result_.metrics["meanErrorM"]=horizontal_.mean;result_.metrics["maxErrorM"]=horizontal_.maximum;result_.metrics["maxAltErrorM"]=vertical_.maximum;result_.metrics["maxTiltDeg"]=tilt_.maximum;if(auto sd=horizontal_.deviation())result_.metrics["errorStdM"]=*sd;}
    if(energy_.covered>0)result_.metrics["telemetryEnergyWh"]=energy_.wh;
    auto v=state_->vehicles.constFind(state_->selected);bool landed=v!=state_->vehicles.cend()&&!v->armed&&v->landed==1&&now-v->landedAt<3;
    if(landed&&now-v->posAt<1){auto end=aero::local(v->lat,v->lon,result_.originLat,result_.originLon);aero::Point target=result_.spec.route.isEmpty()?aero::Point{}:result_.spec.route.back();result_.metrics["landingErrorM"]=aero::distance(end,target);}
    bool enough=coverage>=result_.spec.minCoverage&&horizontal_.count>=10;
    bool exceeded=horizontal_.maximum>result_.spec.maxError||vertical_.maximum>result_.spec.maxAltError||tilt_.maximum>result_.spec.maxTilt;
    if(result_.spec.kind=="takeoff")exceeded|=result_.metrics["takeoffOvershootM"].toDouble()>result_.spec.maxAltError||!result_.metrics.contains("landingErrorM")||result_.metrics["landingErrorM"].toDouble()>result_.spec.maxError;
    if(!result_.spec.expectedMode.isEmpty())exceeded|=!modeObserved_;
    bool sensorRequested=false;for(auto e:result_.spec.events)sensorRequested|=e.type=="sensor";
    if(sensorRequested&&!sensorObserved_){enough=false;fatal_+=" Sensör komutu kabul edildi ancak etkisi telemetriyle doğrulanamadı.";}
    if(result_.spec.energyRequired){enough=false;fatal_+=" Bu arka uçta kalibre edilmiş fiziksel enerji modeli doğrulanmıyor; Wh yalnız telemetri integralidir.";}
    result_.status=QString::fromStdString(aero::result(complete_&&landed&&!unsafe_&&fatal_.isEmpty(),enough,exceeded));
    result_.reason=!fatal_.isEmpty()?fatal_:!enough?"Ölçüm kapsamı / olay etkisi yeterli değil.":exceeded?"Tanımlanan eşik veya beklenen uçuş modu sağlanmadı.":"Uçuş, olay temizliği, iniş ve parametre geri yüklemeleri doğrulandı.";
    note("finish",result_.status+": "+result_.reason,result_.status=="PASS");emit finished(result_);
    if(stopping_||unsafe_||!landed||++runIndex_>=specs_.size()){phase_=Phase::Idle;emit changed();emit batchFinished();}else setPhase(Phase::Between,now);
}
