#include "SessionRecording.hpp"
#include "FlightViews.hpp"
#include "Scene3DView.hpp"
#include "CameraPanel.hpp"
#include <QTabWidget>
#include <QSignalBlocker>
#include "ExperimentCore.hpp"
#include <QJsonDocument>
#include <QSet>
#include <QSaveFile>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <algorithm>
#include <cmath>
namespace {
QJsonObject frame(const FlightState&s,double now,double t){
    QJsonArray vehicles;for(auto i=s.vehicles.begin();i!=s.vehicles.end();++i){auto v=i.value();vehicles.append(QJsonObject{{"system",i.key()},{"lat",v.lat},{"lon",v.lon},{"alt",v.alt},{"amsl",v.amsl},{"roll",v.roll},{"pitch",v.pitch},{"yaw",v.yaw},{"battery",v.battery},{"armed",v.armed},{"px4",v.px4},{"landed",v.landed},{"mode",double(v.customMode)},{"bootMs",double(v.bootMs)},{"posAge",v.posAt>0?now-v.posAt:1e6},{"attAge",v.attitudeAt>0?now-v.attitudeAt:1e6},{"hbAge",v.heartbeatAt>0?now-v.heartbeatAt:1e6},{"landAge",v.landedAt>0?now-v.landedAt:1e6},{"batteryAge",v.batteryAt>0?now-v.batteryAt:1e6}});}return {{"t",t},{"vehicles",vehicles}};
}
bool validFrame(const QJsonObject&o){if(!o["t"].isDouble()||!std::isfinite(o["t"].toDouble())||o["t"].toDouble()<0||o["t"].toDouble()>86400||!o["vehicles"].isArray())return false;auto a=o["vehicles"].toArray();if(a.size()>3)return false;QSet<int>ids;
    for(auto item:a){auto v=item.toObject();int sys=v["system"].toInt();if(sys<1||sys>3||ids.contains(sys)||v["system"].toDouble()!=sys)return false;ids.insert(sys);for(auto key:{"lat","lon","alt","amsl","roll","pitch","yaw","battery","landed","mode","bootMs","posAge","attAge","hbAge","landAge","batteryAge"})if(!v[key].isDouble()||!std::isfinite(v[key].toDouble()))return false;for(auto key:{"roll","pitch","yaw"})if(std::abs(v[key].toDouble())>3600)return false;for(auto key:{"alt","amsl"})if(std::abs(v[key].toDouble())>100000)return false;if(std::abs(v["lat"].toDouble())>85||std::abs(v["lon"].toDouble())>180||v["battery"].toDouble() < -1||v["battery"].toDouble()>100||v["landed"].toInt()<0||v["landed"].toInt()>4||v["mode"].toDouble()<0||v["mode"].toDouble()>4294967295.||v["bootMs"].toDouble()<0||v["bootMs"].toDouble()>4294967295.||!v["armed"].isBool()||!v["px4"].isBool())return false;for(auto key:{"posAge","attAge","hbAge","landAge","batteryAge"})if(v[key].toDouble()<0)return false;}return true;
}
}
bool SessionRecording::start(const QString&p,const QJsonObject&m){if(recording_||p.isEmpty())return false;path_=p;metadata_=m;frames_={};events_={};eventBytes_=0;start_=sitl::monoSeconds();last_=0;recording_=true;return true;}
bool SessionRecording::capture(const FlightState&s,double now){if(!recording_||now-last_<.1)return true;last_=now;if(frames_.size()>=18000){return false;}frames_.append(frame(s,now,std::max(0.,now-start_)));return true;}
void SessionRecording::event(const QString&text,double now){if(recording_&&events_.size()<10000&&eventBytes_<2*1024*1024){auto bounded=text.left(2048);eventBytes_+=bounded.toUtf8().size();events_.append(QJsonObject{{"t",std::max(0.,now-start_)},{"text",bounded}});}}
bool SessionRecording::save(QString&e){if(!recording_)return true;QSaveFile f(path_);auto bytes=QJsonDocument(QJsonObject{{"schema",1},{"type","aerocore-session"},{"metadata",metadata_},{"frames",frames_},{"events",events_}}).toJson(QJsonDocument::Compact);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){e=f.errorString();return false;}recording_=false;return true;}
std::optional<QJsonObject>SessionRecording::load(const QString&p,QString&e){QFile f(p);if(!f.open(QIODevice::ReadOnly)||f.size()>64*1024*1024){e="Kayıt okunamadı / 64 MiB sınırı aşıldı.";return {};}QJsonParseError pe;auto doc=QJsonDocument::fromJson(f.readAll(),&pe);auto o=doc.object();auto a=o["frames"].toArray();if(pe.error!=QJsonParseError::NoError||o["schema"].toInt()!=1||o["type"].toString()!="aerocore-session"||a.isEmpty()||a.size()>40000||!o["events"].isArray()||o["events"].toArray().size()>50000){e="Geçersiz oturum kayıt biçimi.";return {};}
    double previous=-1;for(auto value:a){auto fr=value.toObject();if(!validFrame(fr)||fr["t"].toDouble()<previous){e="Kare / zaman sırası geçersiz.";return {};}previous=fr["t"].toDouble();}
    previous=-1;for(auto value:o["events"].toArray()){auto ev=value.toObject();double t=ev["t"].toDouble(-1);if(!std::isfinite(t)||t<previous||t<0||t>86400||!ev["text"].isString()||ev["text"].toString().size()>8192){e="Olay kaydı geçersiz.";return {};}previous=t;}return o;
}
bool SessionRecording::applyFrame(const QJsonObject&o,FlightState&s,double now){if(!validFrame(o))return false;QMap<int,FlightVehicle>vehicles;for(auto item:o["vehicles"].toArray()){auto v=item.toObject();FlightVehicle x;x.lat=v["lat"].toDouble();x.lon=v["lon"].toDouble();x.alt=v["alt"].toDouble();x.amsl=v["amsl"].toDouble();x.roll=v["roll"].toDouble();x.pitch=v["pitch"].toDouble();x.yaw=v["yaw"].toDouble();x.battery=v["battery"].toInt();x.armed=v["armed"].toBool();x.px4=v["px4"].toBool();x.landed=v["landed"].toInt();x.customMode=quint32(v["mode"].toDouble());x.bootMs=quint32(v["bootMs"].toDouble());auto stamp=[&](const char*k){double age=v[k].toDouble();return age>=1e6?0.:now-age;};x.posAt=stamp("posAge");x.attitudeAt=stamp("attAge");x.heartbeatAt=stamp("hbAge");x.landedAt=stamp("landAge");x.batteryAt=stamp("batteryAge");vehicles[v["system"].toInt()]=x;}s.vehicles=vehicles;return true;}
ReplayPanel::ReplayPanel(QWidget*p):QWidget(p),state_(std::make_shared<FlightState>()){
    setObjectName("replayPanel");auto*l=new QVBoxLayout(this);auto*h=new QHBoxLayout;auto*openButton=new QPushButton("Oturum kaydı aç");play_=new QPushButton("Oynat");play_->setObjectName("replayPlayButton");speed_=new QComboBox;for(double n:{.5,1.,2.,4.})speed_->addItem(QString::number(n)+"×",n);speed_->setCurrentIndex(1);systemSpin_=new QSpinBox;systemSpin_->setRange(1,3);systemSpin_->setObjectName("replaySystemSpin");h->addWidget(openButton);h->addWidget(play_);h->addWidget(speed_);h->addWidget(new QLabel("SYS"));h->addWidget(systemSpin_);status_=new QLabel("KAYIT OYNATMA • Kaydedilmiş veri • Uçuş komutu göndermez");status_->setWordWrap(true);h->addWidget(status_,1);l->addLayout(h);
    auto*split=new QSplitter;auto*views=new QTabWidget;views->setObjectName("replayViews");map_=new MapView(state_);map_->setOnline(false);scene_=new Scene3DView(state_);views->addTab(map_,"Harita");views->addTab(scene_,"3B SAHNE");split->addWidget(views);
    auto*right=new QTabWidget;drone_=new DroneView(state_);camera_=new CameraPanel(this);right->addTab(drone_,"Drone modeli");right->addTab(camera_,"Video kaynağı");split->addWidget(right);l->addWidget(split,1);
    scene_->selected=[this](int sys){systemSpin_->setValue(sys);return true;};
    slider_=new QSlider(Qt::Horizontal);slider_->setObjectName("replaySlider");slider_->setRange(0,10000);l->addWidget(slider_);eventsView_=new QPlainTextEdit;eventsView_->setReadOnly(true);eventsView_->setMaximumHeight(110);l->addWidget(eventsView_);
    timer_.setInterval(50);connect(&timer_,&QTimer::timeout,this,[this]{double now=sitl::monoSeconds();cursor_=std::min(duration_,cursor_+(now-lastTick_)*speed_->currentData().toDouble());lastTick_=now;showFrame();if(cursor_>=duration_){timer_.stop();play_->setText("Oynat");camera_->setReplay(cursor_,false,speed_->currentData().toDouble());}});
    connect(openButton,&QPushButton::clicked,this,&ReplayPanel::open);connect(play_,&QPushButton::clicked,this,[this]{if(frames_.isEmpty())return;if(timer_.isActive()){timer_.stop();play_->setText("Oynat");}else{if(cursor_>=duration_)cursor_=0;lastTick_=sitl::monoSeconds();timer_.start();play_->setText("Duraklat");}showFrame();});connect(slider_,&QSlider::sliderMoved,this,[this](int n){seek(duration_*n/10000.);});connect(systemSpin_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int n){state_->selected=n;map_->followDrone();scene_->followSelected();camera_->setSystem(n);showFrame();});
}
void ReplayPanel::open(){auto p=QFileDialog::getOpenFileName(this,"Oturum kaydı aç",{},"AeroCore JSON (*.json)");if(p.isEmpty())return;QString e;auto o=SessionRecording::load(p,e);if(!o){QMessageBox::warning(this,"Kayıt",e);return;}timer_.stop();play_->setText("Oynat");frames_=(*o)["frames"].toArray();events_=(*o)["events"].toArray();duration_=frames_.last().toObject()["t"].toDouble();title_=p;map_->setPlan({});scene_->setPlan({});scene_->resetOrigin();
    auto route=(*o)["metadata"].toObject()["routePlan"].toArray();QVector<QPointF> planned;
    for(auto point:route){auto coords=point.toArray();if(coords.size()==2&&coords[0].isDouble()&&coords[1].isDouble())planned.append({coords[0].toDouble(),coords[1].toDouble()});}
    map_->setPlan(planned);scene_->setPlan(planned);seek(0);}
void ReplayPanel::seek(double t){cursor_=std::clamp(t,0.,duration_);lastTick_=sitl::monoSeconds();showFrame();camera_->setReplay(cursor_,timer_.isActive(),speed_->currentData().toDouble(),true);}
void ReplayPanel::showFrame(){if(frames_.isEmpty())return;int lo=0,hi=frames_.size();while(lo<hi){int mid=(lo+hi)/2;if(frames_[mid].toObject()["t"].toDouble()<=cursor_)lo=mid+1;else hi=mid;}int index=std::max(0,lo-1);double now=sitl::monoSeconds();SessionRecording::applyFrame(frames_[index].toObject(),*state_,now);QMap<int,QVector<QPointF>>trails;
    for(int i=std::max(0,index-599);i<=index;++i)for(auto item:frames_[i].toObject()["vehicles"].toArray()){auto v=item.toObject();if(v["posAge"].toDouble()<3)trails[v["system"].toInt()].append({v["lon"].toDouble(),v["lat"].toDouble()});}
    for(auto i=state_->vehicles.begin();i!=state_->vehicles.end();++i)i->trail=trails[i.key()];map_->refresh();drone_->update();scene_->update();camera_->setReplay(cursor_,timer_.isActive(),speed_->currentData().toDouble());slider_->setValue(duration_>0?int(cursor_/duration_*10000):0);status_->setText(QString("KAYIT • %1 / %2 sn • SYS %3 • %4").arg(cursor_,0,'f',1).arg(duration_,0,'f',1).arg(state_->selected).arg(title_));QString text;int count=0;for(int i=events_.size()-1;i>=0&&count<60;--i){auto e=events_[i].toObject();if(e["t"].toDouble()<=cursor_){text.prepend(QString::number(e["t"].toDouble(),'f',2)+" sn • "+e["text"].toString()+"\n");++count;}}eventsView_->setPlainText(text);
}
void ReplayPanel::openResult(const LabResult&r){timer_.stop();play_->setText("Oynat");frames_={};events_={};int sys=std::clamp(r.metrics["systemId"].toInt(1),1,3);state_->selected=sys;{QSignalBlocker b(systemSpin_);systemSpin_->setValue(sys);}camera_->setSystem(sys);scene_->resetOrigin();title_=r.spec.name;for(auto sample:r.samples){FlightState s;s.selected=sys;auto &v=s.vehicles[sys];auto geo=aero::geographic({sample.east,sample.north},r.originLat,r.originLon);v.lat=geo.north;v.lon=geo.east;v.alt=sample.alt;v.roll=sample.roll;v.pitch=sample.pitch;v.yaw=sample.yaw;v.battery=sample.battery;v.armed=true;v.px4=true;v.landed=2;v.customMode=sample.mode;v.bootMs=quint32(sample.boot*1000);double now=sitl::monoSeconds();v.posAt=v.attitudeAt=v.heartbeatAt=v.landedAt=v.batteryAt=now-(sample.measured?0:5);frames_.append(frame(s,now,sample.t));}QVector<QPointF>plan;plan.append({r.originLon,r.originLat});for(auto point:r.spec.route){auto geo=aero::geographic(point,r.originLat,r.originLon);plan.append({geo.east,geo.north});}map_->setPlan(plan);scene_->setPlan(plan);duration_=frames_.isEmpty()?0:frames_.last().toObject()["t"].toDouble();title_+=" • Ölçüm aralığı; ARM durumu kayıtta yok, model gösterimi varsayımsal";
    // Experiment event timestamps are relative to run start; convert using first measured event.
    double offset=r.metrics["measurementStartS"].toDouble();for(auto event:r.eventLog){auto e=event.toObject();if(e["type"].toString()=="measure"){offset=e["t"].toDouble();break;}}for(auto event:r.eventLog){auto e=event.toObject();e["t"]=e["t"].toDouble()-offset;e["text"]=e["type"].toString()+": "+e["text"].toString();events_.append(e);}seek(0);
}
