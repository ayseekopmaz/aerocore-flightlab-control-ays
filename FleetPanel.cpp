#include "FleetPanel.hpp"
#include "FlightViews.hpp"
#include <QTimer>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QFileDialog>
#include <QSaveFile>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <cmath>
QJsonObject VehicleProfile::json()const{QJsonObject p;for(auto i=parameters.begin();i!=parameters.end();++i)p[i.key()]=i.value();return {{"system",system},{"name",name},{"profile",physical},{"parameters",p},{"batteryModel",batteryModel}};}
std::optional<VehicleProfile> VehicleProfile::fromJson(const QJsonObject&o,QString&e){
    VehicleProfile v;v.system=o["system"].toInt();v.name=o["name"].toString().trimmed();v.batteryModel=o["batteryModel"].toString();
    if(v.system<1||v.system>3||o["system"].toDouble()!=v.system||v.name.isEmpty()||v.name.size()>60||v.batteryModel.size()>120||!o["profile"].isObject()||!o["parameters"].isObject()){e="Araç kimliği / profil biçimi geçersiz.";return {};}
    v.physical=o["profile"].toObject();if(v.physical.size()!=5){e="Fizik profili beş alan içermeli.";return {};}
    for(auto key:{"payload","cgX","cgY","cgZ","gpsNoise"})if(!v.physical[key].isDouble()||!std::isfinite(v.physical[key].toDouble())){e="Fizik profilinde geçersiz sayı.";return {};}
    LabSpec s;v.apply(s);e=s.validate();if(!e.isEmpty())return {};
    auto params=o["parameters"].toObject();if(params.size()>30){e="En fazla 30 parametre.";return {};}
    for(auto i=params.begin();i!=params.end();++i){if(!QRegularExpression("^[A-Z][A-Z0-9_]{0,15}$").match(i.key()).hasMatch()||i.key()=="MAV_SYS_ID"||i.key()=="SYS_AUTOSTART"||!i.value().isDouble()||!std::isfinite(i.value().toDouble())||std::abs(i.value().toDouble())>1e6){e="Geçersiz veya ayrılmış parametre: "+i.key();return {};}v.parameters[i.key()]=i.value().toDouble();}
    if(v.parameters.contains("SIM_BAT_DRAIN")&&(v.parameters["SIM_BAT_DRAIN"]<0||v.parameters["SIM_BAT_DRAIN"]>3600)){e="Batarya süre sınırı 0–3600 sn.";return {};}
    if(v.parameters.contains("SIM_BAT_MIN_PCT")&&(v.parameters["SIM_BAT_MIN_PCT"]<0||v.parameters["SIM_BAT_MIN_PCT"]>100)){e="Batarya alt sınırı 0–100%.";return {};}
    return v;
}
void VehicleProfile::apply(LabSpec&s)const{
    s.payload=physical["payload"].toDouble();s.cgX=physical["cgX"].toDouble();s.cgY=physical["cgY"].toDouble();s.cgZ=physical["cgZ"].toDouble();s.gpsNoise=physical["gpsNoise"].toDouble();s.batteryModel=batteryModel;
    for(auto i=parameters.begin();i!=parameters.end();++i)s.parameters[i.key()]=i.value();
    if(parameters.contains("SIM_BAT_DRAIN"))s.batteryDrain=parameters["SIM_BAT_DRAIN"];
    if(parameters.contains("SIM_BAT_MIN_PCT"))s.batteryMin=parameters["SIM_BAT_MIN_PCT"];
}
FleetPanel::FleetPanel(QWidget*p):QWidget(p){
    setObjectName("fleetPanel");auto*l=new QVBoxLayout(this);auto*h=new QHBoxLayout;count_=new QSpinBox;count_->setObjectName("fleetCount");count_->setRange(1,3);count_->setValue(3);h->addWidget(new QLabel("Drone sayısı"));h->addWidget(count_);
    auto*saveButton=new QPushButton("Profilleri kaydet");auto*loadButton=new QPushButton("Profilleri aç");auto*compare=new QPushButton("Araçları aynı senaryoda karşılaştır");compare->setObjectName("compareFleetButton");h->addWidget(saveButton);h->addWidget(loadButton);h->addWidget(compare);h->addStretch();l->addLayout(h);
    table_=new QTableWidget(3,11);table_->setObjectName("fleetProfilesTable");table_->setHorizontalHeaderLabels({"SYS","Araç adı","Yük kg","CG X m","CG Y m","CG Z m","GPS σ m","Batarya süre sn","Alt sınır %","Batarya modeli","Kontrol parametreleri JSON"});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table_->horizontalHeader()->setStretchLastSection(true);table_->setMinimumHeight(150);table_->setMaximumHeight(170);
    for(int r=0;r<3;++r)for(int c=0;c<11;++c){QString t=c==0?QString::number(r+1):c==1?QString("Drone %1").arg(r+1):c==8?"10":c==9?"PX4 synthetic":c==10?"{}":"0";auto*i=new QTableWidgetItem(t);if(c==0)i->setFlags(i->flags()&~Qt::ItemIsEditable);table_->setItem(r,c,i);}
    l->addWidget(table_);notice_=new QLabel("Ayrı PX4 oturumları • Başlangıçlar 8 m aralıklı • Fizik profilleri açılışta uygulanır. Batarya süre parametresi tüketim benzetimidir; kalibre edilmiş Wh modeli değildir.");notice_->setWordWrap(true);l->addWidget(notice_);
    connect(count_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int n){for(int i=0;i<3;++i)table_->setRowHidden(i,i>=n);});
    connect(saveButton,&QPushButton::clicked,this,&FleetPanel::save);connect(loadButton,&QPushButton::clicked,this,&FleetPanel::load);connect(compare,&QPushButton::clicked,this,&FleetPanel::compareRequested);
}
int FleetPanel::count()const{return count_->value();}
void FleetPanel::setActive(bool active){count_->setEnabled(!active);table_->setEnabled(!active);}
QJsonArray FleetPanel::profiles(QString&e)const{
    QJsonArray out;for(int r=0;r<count();++r){auto text=[&](int c){return table_->item(r,c)?table_->item(r,c)->text().trimmed():QString{};};VehicleProfile v;v.system=r+1;v.name=text(1);v.batteryModel=text(9);int c=2;for(auto k:{"payload","cgX","cgY","cgZ","gpsNoise"}){bool ok=false;double n=text(c++).toDouble(&ok);if(!ok||!std::isfinite(n)){e="SYS "+QString::number(r+1)+": geçersiz fizik değeri.";return {};}v.physical[k]=n;}
        QJsonParseError parse;auto doc=QJsonDocument::fromJson(text(10).toUtf8(),&parse);if(parse.error!=QJsonParseError::NoError||!doc.isObject()){e="Kontrol parametreleri JSON nesnesi olmalı: {\"MPC_XY_P\":0.95}";return {};}
        auto o=v.json();auto params=doc.object();for(int col:{7,8}){bool ok=false;double n=text(col).toDouble(&ok);if(!ok||!std::isfinite(n)){e="Geçersiz batarya değeri.";return {};}params[col==7?"SIM_BAT_DRAIN":"SIM_BAT_MIN_PCT"]=n;}o["parameters"]=params;
        auto parsed=VehicleProfile::fromJson(o,e);if(!parsed)return {};out.append(parsed->json());}return out;
}
void FleetPanel::save(){QString e;auto p=profiles(e);if(!e.isEmpty()){QMessageBox::warning(this,"Profiller",e);return;}auto path=QFileDialog::getSaveFileName(this,"Araç profillerini kaydet","AeroCore_Fleet.json","JSON (*.json)");if(path.isEmpty())return;QSaveFile f(path);auto bytes=QJsonDocument(QJsonObject{{"schema",1},{"vehicles",p}}).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())QMessageBox::warning(this,"Profiller",f.errorString());}
void FleetPanel::load(){if(!table_->isEnabled()){QMessageBox::information(this,"Profiller","Oturumu kapattıktan sonra profil yükleyin.");return;}auto path=QFileDialog::getOpenFileName(this,"Araç profillerini aç",{},"JSON (*.json)");if(path.isEmpty())return;QFile f(path);QString e;if(!f.open(QIODevice::ReadOnly)||f.size()>65536){QMessageBox::warning(this,"Profiller","Dosya okunamadı veya 64 KiB sınırını aştı.");return;}auto o=QJsonDocument::fromJson(f.readAll()).object();auto a=o["vehicles"].toArray();QVector<VehicleProfile> parsed;if(o["schema"].toInt()!=1||a.isEmpty()||a.size()>3)e="Profil biçimi geçersiz.";else for(int i=0;i<a.size();++i){auto v=VehicleProfile::fromJson(a[i].toObject(),e);if(!v||v->system!=i+1){if(e.isEmpty())e="SYS sıralaması geçersiz.";break;}parsed.append(*v);}if(!e.isEmpty()){QMessageBox::warning(this,"Profiller",e);return;}
    count_->setValue(parsed.size());for(int r=0;r<parsed.size();++r){auto v=parsed[r];table_->item(r,1)->setText(v.name);int c=2;for(auto k:{"payload","cgX","cgY","cgZ","gpsNoise"})table_->item(r,c++)->setText(QString::number(v.physical[k].toDouble()));table_->item(r,7)->setText(QString::number(v.parameters.value("SIM_BAT_DRAIN",0)));table_->item(r,8)->setText(QString::number(v.parameters.value("SIM_BAT_MIN_PCT",10)));table_->item(r,9)->setText(v.batteryModel);v.parameters.remove("SIM_BAT_DRAIN");v.parameters.remove("SIM_BAT_MIN_PCT");table_->item(r,10)->setText(QString::fromUtf8(QJsonDocument(v.json()["parameters"].toObject()).toJson(QJsonDocument::Compact)));}
}

void FleetPanel::bindState(std::shared_ptr<FlightState>s){
    auto*row=new QHBoxLayout;static_cast<QVBoxLayout*>(layout())->addLayout(row);
    for(int sys=1;sys<=3;++sys){auto*box=new QWidget;box->setMaximumHeight(310);auto*l=new QVBoxLayout(box);auto*status=new QLabel(QString("SYS %1 • bağlantı bekleniyor").arg(sys));status->setWordWrap(true);l->addWidget(status);
        auto local=std::make_shared<FlightState>();local->selected=sys;auto*view=new DroneView(local);view->setMinimumSize(240,160);view->setMaximumHeight(230);l->addWidget(view);row->addWidget(box);
        auto*t=new QTimer(box);t->setInterval(100);connect(t,&QTimer::timeout,box,[=,this]{local->vehicles=s->vehicles;box->setVisible(sys<=count());auto i=s->vehicles.constFind(sys);double now=sitl::monoSeconds();bool live=i!=s->vehicles.cend()&&i->heartbeatAt>0&&now-i->heartbeatAt<3;bool warning=!live||(i->battery>=0&&now-i->batteryAt<5&&i->battery<20)||(now-i->posAt<3&&i->alt>120);
            status->setText(QString("SYS %1 • %2 • %3").arg(sys).arg(table_->item(sys-1,1)->text()).arg(!live?"BAĞLANTI YOK":i->armed?"ARMED":"YER / DISARMED")+(live?QString("\nİrtifa %1 m • Batarya %2%").arg(i->alt,0,'f',1).arg(i->battery):QString{}));status->setStyleSheet(warning?"background:#FEF2F2;color:#B91C1C;padding:8px":"background:#E7F0FB;color:#17365D;padding:8px");view->update();});t->start();
    }
    static_cast<QVBoxLayout*>(layout())->addStretch(1);
}
