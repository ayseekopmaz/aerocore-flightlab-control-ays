#include "HardwarePanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <array>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <cmath>
namespace {
class RelayLiveView final:public QWidget {
public:
    QJsonArray rows;std::array<QVector<QPointF>,3> curves;
    explicit RelayLiveView(QWidget*p):QWidget(p){setMinimumHeight(145);}
    void accept(const QJsonArray&r){rows=r;for(auto item:r){auto o=item.toObject();int n=o["system"].toInt()-1;if(n<0||n>2)continue;double t=o["timeS"].toDouble();if(!curves[n].isEmpty()&&t<curves[n].last().x())curves[n].clear();curves[n].append({t,o["socPct"].toDouble()});if(curves[n].size()>4000)curves[n].removeFirst();}update();}
protected:
    void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#F3F7FB"));p.setRenderHint(QPainter::Antialiasing);const std::array<QColor,3> colors{QColor("#2563EB"),QColor("#DC2626"),QColor("#16A34A")};
        p.setPen(QColor("#17365D"));p.drawText(8,18,"IHA Maker deneyleri • canlı tahmini SOC / akım / sabit gerilim — elektriksel ölçüm değildir");
        int row=0;for(auto item:rows){auto o=item.toObject();p.setPen(colors[std::min(row,2)]);p.drawText(8,38+row*21,QString("SYS %1  %2  |  %3%  |  %4 A  |  %5 V").arg(o["system"].toInt()).arg(o["role"].toString()).arg(o["socPct"].toDouble(),0,'f',1).arg(o["currentA"].toDouble(),0,'f',2).arg(o["voltageV"].toDouble(),0,'f',1));++row;}
        QRectF plot(width()*.53,30,width()*.44,height()-48);p.setPen(QColor("#94A3B8"));p.drawRect(plot);double maxT=1;for(const auto &c:curves)if(!c.isEmpty())maxT=std::max(maxT,c.last().x());for(int n=0;n<3;++n){QPainterPath path;bool first=true;for(auto point:curves[n]){QPointF q(plot.left()+point.x()/maxT*plot.width(),plot.bottom()-point.y()/100*plot.height());if(first){path.moveTo(q);first=false;}else path.lineTo(q);}p.setPen(QPen(colors[n],2));p.drawPath(path);}}
};
}
void HardwarePanel::field(QFormLayout *form,const QString &key,const QString &label,double low,double high,double value,int decimals){
    auto *spin=new QDoubleSpinBox(this);spin->setObjectName("iha_"+key);spin->setRange(low,high);spin->setDecimals(decimals);spin->setValue(value);fields_[key]=spin;form->addRow(label,spin);
    connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{assess();});
}
HardwarePanel::HardwarePanel(QWidget *parent):QWidget(parent){
    setObjectName("hardwarePanel");auto *layout=new QVBoxLayout(this);
    auto *note=new QLabel("Atölye/PDF değerlerini birimleriyle gir. Gazebo modeli bu değerlerden oluşturulur. Değişiklikler sonraki Başlat işleminde uygulanır.",this);note->setWordWrap(true);layout->addWidget(note);
    auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto *maker=new QPushButton("Atölyeyi aç",this),*load=new QPushButton("Profil yükle",this),*save=new QPushButton("Profili kaydet",this);
    for(auto *b:{maker,load,save})buttons->addWidget(b);auto *exportPdf=new QPushButton("Deney analizini PDF al",this);exportPdf->setObjectName("ihaExportResultPdf");buttons->addWidget(exportPdf);connect(exportPdf,&QPushButton::clicked,this,&HardwarePanel::exportResultsPdfRequested);buttons->addStretch();
    auto *missionBar=new QHBoxLayout;missionMode_=new QComboBox(this);missionMode_->addItem("Üç ayrı yön görevi");missionMode_->addItem("Görev İhale Deneyi");missionMode_->addItem("Rüzgar Hamlesinde Dinamik Sürü");missionBar->addWidget(missionMode_);startMission_=new QPushButton("IHA Maker görevini başlat",this);startMission_->setObjectName("ihaStartButton");stopMission_=new QPushButton("Görevi iptal et / iniş",this);stopMission_->setEnabled(false);missionBar->addWidget(startMission_);missionBar->addWidget(stopMission_);missionStatus_=new QLabel("Üç drone: doğu / kuzey / batı rotası; ortak 60 saniye ölçüm",this);missionStatus_->setWordWrap(true);missionBar->addWidget(missionStatus_,1);layout->addLayout(missionBar);
    relayView_=new RelayLiveView(this);relayView_->hide();layout->addWidget(relayView_);
    connect(missionMode_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int n){relayView_->setVisible(n!=0);missionStatus_->setText(n==2?"V-formasyonu: sabit referans + adaptif koşu; ortak rüzgar ve tahmini tüketim":n==1?"Görev İhale Deneyi: 15 dk test; %25 devir eşiği; 30. saniyede 3 m/s ortak rüzgar":"Üç drone: doğu / kuzey / batı rotası; ortak 60 saniye ölçüm");});
    missionMode_->setCurrentIndex(1);
    connect(startMission_,&QPushButton::clicked,this,[this]{use_->setChecked(true);emit startIhaRequested();});
    connect(stopMission_,&QPushButton::clicked,this,&HardwarePanel::stopIhaRequested);
    use_=new QCheckBox("Bu özelliklerle Gazebo modeli oluştur",this);use_->setObjectName("ihaUseModel");use_->setChecked(true);layout->addWidget(use_);
    auto *scroll=new QScrollArea(this);scroll->setWidgetResizable(true);auto *body=new QWidget;auto *groups=new QHBoxLayout(body);
    auto *physical=new QGroupBox("Gövde ve itki",body);auto *form=new QFormLayout(physical);
    field(form,"mass","Toplam kalkış ağırlığı (g)",10,10000,599);
    field(form,"span","Karşı motor merkezleri arası (mm)",50,2000,220);
    field(form,"diameter","Pervane çapı (inç)",.1,30,5.1);
    field(form,"pitch","Pervane hatvesi (inç)",.1,30,4.66);
    field(form,"blades","Pervane kanat sayısı",2,6,3,0);
    field(form,"rotorMass","Tek pervane ağırlığı (g)",.01,100,4.6);
    field(form,"motorMass","Tek motor ağırlığı (g)",1,1000,32);
    field(form,"thrust","Dört motor toplam maksimum itki (g)",.1,100000,7318);
    field(form,"rpm","Maksimum pervane devri (rpm)",100,200000,39699,0);
    field(form,"cgX","Ağırlık merkezi: ileri (mm)",-500,500,4.5);
    field(form,"cgY","Ağırlık merkezi: sağ (mm)",-500,500,0);
    field(form,"cgZ","Ağırlık merkezi: yukarı (mm)",-500,500,15.1);
    groups->addWidget(physical);
    auto *second=new QWidget(body);auto *secondLayout=new QVBoxLayout(second);
    auto *cameraBox=new QGroupBox("Kamera",second);auto *cameraForm=new QFormLayout(cameraBox);camera_=new QCheckBox("Kamera var",cameraBox);camera_->setObjectName("ihaCameraEnabled");camera_->setChecked(true);cameraForm->addRow(camera_);
    field(cameraForm,"cameraWidth","Görüntü genişliği (px)",64,3840,640,0);field(cameraForm,"cameraHeight","Görüntü yüksekliği (px)",64,2160,480,0);field(cameraForm,"cameraRate","Kare hızı (Hz)",1,60,30,0);
    secondLayout->addWidget(cameraBox);
    auto *advanced=new QGroupBox("Geometri / model varsayımları",second);auto *advancedForm=new QFormLayout(advanced);
    field(advancedForm,"bodyLength","Merkez gövde uzunluğu (mm)",20,500,100);field(advancedForm,"bodyWidth","Merkez gövde genişliği (mm)",20,500,70);field(advancedForm,"bodyHeight","Merkez gövde yüksekliği (mm)",5,200,45);
    field(advancedForm,"tauUp","Motor hızlanma süresi (s)",.001,2,.02,3);field(advancedForm,"tauDown","Motor yavaşlama süresi (s)",.001,2,.04,3);field(advancedForm,"moment","Tork / itki katsayısı (m)",.0001,.2,.016,4);secondLayout->addWidget(advanced);
    auto *electrical=new QGroupBox("Elektrik / tahmini tüketim",second);auto *electricForm=new QFormLayout(electrical);
    field(electricForm,"kv","Motor KV",100,10000,1950,0);field(electricForm,"cells","Batarya hücre sayısı (S)",1,12,6,0);field(electricForm,"capacity","Batarya kapasitesi (mAh)",50,30000,1300,0);field(electricForm,"esc","ESC sürekli akımı / motor (A)",1,300,55);
    field(electricForm,"hoverCurrent","Askı toplam akımı (A)",.01,300,9.8);
    field(electricForm,"voltage","Yük altı gerilim (V)",1,60,21.9);
    field(electricForm,"usable","Kullanılabilir kapasite (%)",1,100,80);
    field(electricForm,"reportDuration","Atölye tahmini askı süresi (sn)",0,7200,383,0);
    auto *electricNote=new QLabel("Uçuş sonrası akım + yatış açılarından tahmini tüketim hesaplanır. Motor akımı ölçümü, gerilim düşmesi veya uçuşa bağlı elektriksel fizik değildir.",electrical);electricNote->setWordWrap(true);electricForm->addRow(electricNote);secondLayout->addWidget(electrical);secondLayout->addStretch();groups->addWidget(second);
    scroll->setWidget(body);layout->addWidget(scroll,1);status_=new QLabel(this);status_->setWordWrap(true);layout->addWidget(status_);
    QFile initial(":/hardware/profiles/iha_maker_v2.json");QString error;if(initial.open(QIODevice::ReadOnly))fill(QJsonDocument::fromJson(initial.readAll()).object(),error);assess();
    connect(camera_,&QCheckBox::toggled,this,[this](bool enabled){for(const auto &key:{"cameraWidth","cameraHeight","cameraRate"})fields_[key]->setEnabled(enabled);assess();});
    connect(maker,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://iha.epertechnologies.com/design/3b6759ad-af27-4ce7-ae82-5f0d1c809a39"));});
    connect(load,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"Özellik profili",{},"JSON (*.json)");if(path.isEmpty())return;QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1048576){QMessageBox::warning(this,"Profil","Dosya okunamadı veya çok büyük.");return;}QJsonParseError parse;auto doc=QJsonDocument::fromJson(f.readAll(),&parse);QString error;if(parse.error!=QJsonParseError::NoError||!doc.isObject()||!fill(doc.object(),error))QMessageBox::warning(this,"Profil",error.isEmpty()?"Geçerli profil gerekli.":error);});
    connect(save,&QPushButton::clicked,this,[this]{QString error;const auto profile=simulationProfile(error);if(!error.isEmpty()){QMessageBox::warning(this,"Profil",error);return;}auto path=QFileDialog::getSaveFileName(this,"Özellik profilini kaydet","iha_maker_ozellikler.json","JSON (*.json)");if(path.isEmpty())return;if(!path.endsWith(".json",Qt::CaseInsensitive))path+=".json";QSaveFile f(path);const auto bytes=QJsonDocument(profile).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())QMessageBox::warning(this,"Profil",f.errorString());});
}
bool HardwarePanel::enabledForSimulation() const{return use_->isChecked();}
QJsonObject HardwarePanel::simulationProfile(QString &error) const{
    error.clear();const auto v=[this](const QString &key){return fields_[key]->value();};
    if(v("rotorMass")*4+v("motorMass")*4>=v("mass")){error="Toplam ağırlık, dört motor ve dört pervanenin toplamından büyük olmalı.";return {};}
    if(v("diameter")*25.4>=v("span")/std::sqrt(2.)){error="Bu X yerleşiminde pervaneler çakışıyor. Çapı veya aks açıklığını değiştir.";return {};}
    QJsonObject estimates{{"takeoffMassG",v("mass")},{"wheelbaseMm",v("span")},{"maxTotalThrustG",v("thrust")},{"maxRpm",v("rpm")},{"cgMmForwardRightUp",QJsonArray{v("cgX"),v("cgY"),v("cgZ")}}, {"batteryCells",v("cells")},{"capacityMah",v("capacity")}};
    QJsonObject hw{{"propellerDiameterIn",v("diameter")},{"propellerPitchIn",v("pitch")},{"propellerBlades",v("blades")},{"motorMassEachG",v("motorMass")},{"motorKv",v("kv")},{"escContinuousCurrentEachA",v("esc")},{"cameraEnabled",camera_->isChecked()}};
    QJsonObject sim{{"rotorMassEachG",v("rotorMass")},{"motorTimeConstantUpS",v("tauUp")},{"motorTimeConstantDownS",v("tauDown")},{"momentConstantM",v("moment")},{"bodyDimensionsMm",QJsonArray{v("bodyLength"),v("bodyWidth"),v("bodyHeight")}}, {"cameraWidth",v("cameraWidth")},{"cameraHeight",v("cameraHeight")},{"cameraRateHz",v("cameraRate")},{"assumptions",QJsonArray{"Simetrik X geometri; parça konumları yaklaşık","Atalet: merkezi kutu ve motor nokta kütlelerinden yaklaşık hesap","İtki k*omega^2; girilen maksimum noktaya uyum","Motor yanıt/tork katsayısı varsayım; ölçümle güncellenebilir","Hatve/kanat sayısı kayıttır; girilen itki/devirden bağımsız yeni katsayı türetilmez","KV/batarya/ESC elektriksel modeli yok; NavSat simülasyon sensörüdür"}}};
    QJsonObject report=reference_;for(auto i=estimates.cbegin();i!=estimates.cend();++i)report[i.key()]=i.value();report["hoverDurationSeconds"]=v("reportDuration");
    return {{"schemaVersion",1},{"source",QJsonObject{{"tool","IHA Maker"}}},{"referenceReport",report},{"energyEstimate",QJsonObject{{"hoverCurrentA",v("hoverCurrent")},{"voltageV",v("voltage")},{"usableFraction",v("usable")/100}}},{"reportEstimates",estimates},{"hardware",hw},{"simulation",sim}};
}
bool HardwarePanel::fill(const QJsonObject &o,QString &error){
    if(o.value("schemaVersion").toInt()!=1){error="schemaVersion=1 gerekli.";return false;}
    const auto e=o.value("reportEstimates").toObject(),h=o.value("hardware").toObject(),s=o.value("simulation").toObject();
    QMap<QString,QJsonValue> values{{"mass",e.value("takeoffMassG")},{"span",e.value("wheelbaseMm")},{"thrust",e.value("maxTotalThrustG")},{"rpm",e.value("maxRpm")},{"diameter",h.value("propellerDiameterIn")},{"pitch",h.value("propellerPitchIn")},{"blades",h.value("propellerBlades")},{"rotorMass",s.value("rotorMassEachG")},{"motorMass",h.value("motorMassEachG")},{"kv",h.value("motorKv")},{"cells",e.value("batteryCells")},{"capacity",e.value("capacityMah")},{"esc",h.value("escContinuousCurrentEachA")},{"tauUp",s.value("motorTimeConstantUpS")},{"tauDown",s.value("motorTimeConstantDownS")},{"moment",s.value("momentConstantM")}};
    // Earlier PDF profiles used motorMassEachG=32 in hardware; keep compatibility.
    if(values["motorMass"].isUndefined())values["motorMass"]=32.;
    const auto energy=o.value("energyEstimate").toObject();
    values["hoverCurrent"]=energy.value("hoverCurrentA").isDouble()?energy.value("hoverCurrentA"):e.value("hoverCurrentA").isDouble()?e.value("hoverCurrentA"):QJsonValue(9.8);
    values["voltage"]=energy.value("voltageV").isDouble()?energy.value("voltageV"):e.value("loadedVoltageV").isDouble()?e.value("loadedVoltageV"):QJsonValue(21.9);
    values["reportDuration"]=o.value("referenceReport").toObject().value("hoverDurationSeconds").isDouble()?o.value("referenceReport").toObject().value("hoverDurationSeconds"):e.value("hoverDurationSeconds").isDouble()?e.value("hoverDurationSeconds"):QJsonValue(0.);
    values["usable"]=energy.value("usableFraction").isDouble()?QJsonValue(energy.value("usableFraction").toDouble()*100):QJsonValue(80.);
    const auto cg=e.value("cgMmForwardRightUp").toArray();if(cg.size()!=3){error="Üç ağırlık merkezi koordinatı gerekli.";return false;}values["cgX"]=cg[0];values["cgY"]=cg[1];values["cgZ"]=cg[2];
    const auto dims=s.value("bodyDimensionsMm").toArray();if(dims.size()==3){values["bodyLength"]=dims[0];values["bodyWidth"]=dims[1];values["bodyHeight"]=dims[2];}else {values["bodyLength"]=100.;values["bodyWidth"]=70.;values["bodyHeight"]=45.;}
    for(const auto key:{"cameraWidth","cameraHeight","cameraRate"}){const QString src=QString(key)=="cameraRate"?"cameraRateHz":key;values[key]=s.value(src).isUndefined()?QJsonValue(QString(key)=="cameraWidth"?640.:QString(key)=="cameraHeight"?480.:30.):s.value(src);}
    for(auto i=values.cbegin();i!=values.cend();++i){auto *field=fields_.value(i.key());if(!i.value().isDouble()||!std::isfinite(i.value().toDouble())||i.value().toDouble()<field->minimum()||i.value().toDouble()>field->maximum()||(field->decimals()==0&&i.value().toDouble()!=std::floor(i.value().toDouble()))){error="Geçersiz özellik: "+i.key();return false;}}
    if(values["rotorMass"].toDouble()*4+values["motorMass"].toDouble()*4>=values["mass"].toDouble()||values["diameter"].toDouble()*25.4>=values["span"].toDouble()/std::sqrt(2.)){error="Kütle veya pervane yerleşimi tutarsız.";return false;}
    for(auto i=values.cbegin();i!=values.cend();++i)fields_[i.key()]->setValue(i.value().toDouble());
    camera_->setChecked(h.value("cameraEnabled").toBool(true));for(const auto key:{"cameraWidth","cameraHeight","cameraRate"})fields_[key]->setEnabled(camera_->isChecked());
    reference_=o.contains("referenceReport")?o.value("referenceReport").toObject():e;assess();return true;
}
void HardwarePanel::assess(){if(!status_||!camera_)return;QString error;simulationProfile(error);if(!error.isEmpty()){status_->setText(error);return;}const double ratio=fields_["thrust"]->value()/fields_["mass"]->value();status_->setText(QString("İtki / ağırlık: %1 : 1 • %2\nGeometri/atalet yaklaşık; kamera %3. Uçuş davranışı simülasyonda test edilir.").arg(ratio,0,'f',2).arg(ratio<=1?"Statik itki yetersiz":"Statik itki ağırlıktan büyük").arg(camera_->isChecked()?"açık":"kapalı"));}

void HardwarePanel::setMissionStatus(const QString&s){missionStatus_->setText(s);}
void HardwarePanel::setMissionBusy(bool busy){startMission_->setEnabled(!busy);missionMode_->setEnabled(!busy);stopMission_->setEnabled(busy);use_->setEnabled(!busy);camera_->setEnabled(!busy);for(auto *f:fields_)f->setEnabled(!busy);if(!busy)for(const auto key:{"cameraWidth","cameraHeight","cameraRate"})fields_[key]->setEnabled(camera_->isChecked());}

bool HardwarePanel::relaySelected()const{return missionMode_->currentIndex()==1;}
void HardwarePanel::setRelayTelemetry(const QJsonArray&r){static_cast<RelayLiveView*>(relayView_)->accept(r);}

bool HardwarePanel::windSelected()const{return missionMode_->currentIndex()==2;}
