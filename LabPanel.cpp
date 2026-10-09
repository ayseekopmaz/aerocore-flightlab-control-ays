#include "LabPanel.hpp"
#include "ui_labpanel.h"
#include "LabChart.hpp"
#include "FleetPanel.hpp"
#include <QTimer>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QSaveFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QHeaderView>
#include <QDateTime>
#include <QRegularExpression>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <cmath>
namespace {
QString number(const QJsonObject&m,const QString&k,int digits=2){return m.contains(k)&&m[k].isDouble()?QString::number(m[k].toDouble(),'f',digits):"—";}
bool writeFile(const QString&path,const QByteArray&bytes,QString&error){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){error=f.errorString();return false;}return true;}
std::optional<QJsonObject> readJson(const QString&path,QString&error){QFile f(path);if(!f.open(QIODevice::ReadOnly)){error=f.errorString();return {};}if(f.size()>32*1024*1024){error="Dosya en fazla 32 MiB olabilir.";return {};}QJsonParseError e;auto doc=QJsonDocument::fromJson(f.readAll(),&e);if(e.error!=QJsonParseError::NoError||!doc.isObject()){error="JSON okunamadı: "+e.errorString();return {};}return doc.object();}
void addRow(QTableWidget*t,const QStringList&values){int row=t->rowCount();t->insertRow(row);for(int col=0;col<values.size();++col)t->setItem(row,col,new QTableWidgetItem(values[col]));}
QString csvQuote(QString s){s.replace('"',"\"\"");return '"'+s+'"';}
QJsonObject comparable(LabSpec s){auto o=s.json();for(auto k:{"name","kind","versionTag"})o.remove(k);return o;}
}
LabPanel::LabPanel(std::shared_ptr<FlightState>s,SimulatorSession*session,LabTransport*t,QWidget*parent):QWidget(parent),ui_(std::make_unique<Ui::LabPanel>()),state_(std::move(s)),session_(session),transport_(t){
    ui_->setupUi(this);runner_=new ExperimentRunner(state_,session,t,this);chart_=new LabChart(this);ui_->chartLayout->addWidget(chart_);ui_->chartLayout->setContentsMargins(0,0,0,0);
    for(const auto&k:{"hover","route","takeoff","wind","payload","battery","sensor","link","controller","regression"})ui_->kindCombo->addItem(labKindName(k),k);
    for(auto*field:{ui_->payloadSpin,ui_->cgXSpin,ui_->cgYSpin,ui_->cgZSpin,ui_->gpsNoiseSpin,ui_->drainSpin,ui_->batteryMinSpin}){field->setReadOnly(true);field->setButtonSymbols(QAbstractSpinBox::NoButtons);field->setToolTip("Bu değer Araç profilleri / çoklu drone sekmesinden ayarlanır.");}
    ui_->batteryModelEdit->setReadOnly(true);
    ui_->vehicleLabel->setText("Gazebo x500 / PX4 SITL • Fizik ve batarya: Araç profilleri sekmesi");ui_->modeCombo->addItem("Mod şartı yok","");for(auto mode:{"Return","Land","Hold"})ui_->modeCombo->addItem(mode,mode);
    for(auto table:{ui_->routeTable,ui_->eventsTable,ui_->paramTable,ui_->resultsTable}){table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);}
    ui_->resultsTable->setMaximumHeight(150);ui_->detailsEdit->setMinimumHeight(60);ui_->resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);ui_->resultsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);ui_->detailsEdit->setReadOnly(true);ui_->detailsEdit->setMaximumHeight(100);
    ui_->speedsEdit->setText("2,3,5");ui_->windsEdit->setText("0,3");fill(labPreset("hover"));
    connect(ui_->presetButton,&QPushButton::clicked,this,[this]{fill(labPreset(ui_->kindCombo->currentData().toString()));});
    connect(ui_->addPointButton,&QPushButton::clicked,this,[this]{addRow(ui_->routeTable,{"10","0"});publishRoute();});
    connect(ui_->removePointButton,&QPushButton::clicked,this,[this]{ui_->routeTable->removeRow(ui_->routeTable->currentRow());publishRoute();});
    connect(ui_->clearRouteButton,&QPushButton::clicked,this,[this]{ui_->routeTable->setRowCount(0);publishRoute();});
    connect(ui_->routeTable,&QTableWidget::itemChanged,this,[this]{publishRoute();});
    connect(ui_->addEventButton,&QPushButton::clicked,this,[this]{addRow(ui_->eventsTable,{"10","5","wind","3","90"});});
    connect(ui_->removeEventButton,&QPushButton::clicked,this,[this]{ui_->eventsTable->removeRow(ui_->eventsTable->currentRow());});
    connect(ui_->addParamButton,&QPushButton::clicked,this,[this]{addRow(ui_->paramTable,{"MPC_XY_P","0.95"});});
    connect(ui_->removeParamButton,&QPushButton::clicked,this,[this]{ui_->paramTable->removeRow(ui_->paramTable->currentRow());});
    connect(ui_->runButton,&QPushButton::clicked,this,[this]{start(false);});connect(ui_->batchButton,&QPushButton::clicked,this,[this]{start(true);});
    connect(ui_->abortButton,&QPushButton::clicked,this,[this]{comparisonCancelled_=true;runner_->abort();});
    connect(runner_,&ExperimentRunner::log,this,&LabPanel::log);connect(runner_,&ExperimentRunner::finished,this,[this](const LabResult&r){result(r);if(comparing_&&r.status=="INCONCLUSIVE")comparisonCancelled_=true;});
    connect(runner_,&ExperimentRunner::batchFinished,this,[this]{if(!comparing_)return;QTimer::singleShot(0,this,[this]{if(comparisonCancelled_){comparing_=false;comparisonQueue_.clear();return;}nextComparison();});});
    auto*replay=new QPushButton("Seçili deney kaydını oynat",this);replay->setObjectName("replayResultButton");ui_->resultsTab->layout()->addWidget(replay);
    connect(replay,&QPushButton::clicked,this,[this]{auto rows=ui_->resultsTable->selectionModel()->selectedRows();if(rows.size()!=1){message("Oynatmak için bir deney seçin.");return;}emit replayResult(results_[rows[0].row()]);});
    connect(ui_->resultsTable,&QTableWidget::itemSelectionChanged,this,&LabPanel::selection);
    connect(ui_->saveButton,&QPushButton::clicked,this,[this]{QString error;auto s=spec(error);if(!error.isEmpty()){message(error);return;}auto path=QFileDialog::getSaveFileName(this,"Senaryoyu kaydet",s.name+".json","Senaryo (*.json)");if(path.isEmpty())return;if(!writeFile(path,QJsonDocument(s.json()).toJson(),error))message(error);});
    connect(ui_->loadButton,&QPushButton::clicked,this,[this]{QString path=QFileDialog::getOpenFileName(this,"Senaryo aç",{},"JSON (*.json)");if(path.isEmpty())return;QString error;auto o=readJson(path,error);if(!o){message(error);return;}auto s=LabSpec::fromJson(*o,error);if(!s){message(error);return;}fill(*s);});
    auto import=[this](bool baseline){QString path=QFileDialog::getOpenFileName(this,"Sonuç JSON aç",{},"JSON (*.json)");if(path.isEmpty())return;QString error;auto o=readJson(path,error);if(!o){message(error);return;}auto r=LabResult::fromJson(*o,error);if(!r){message(error);return;}if(baseline){if(r->status!="PASS"){message("Referans sonuç PASS olmalı.");return;}baseline_=*r;ui_->baselineLabel->setText("Referans: "+r->spec.name+" / "+r->spec.versionTag+" / "+r->firmware);}else{if(results_.size()>=200)results_.removeFirst();results_.append(*r);refreshResults();}};
    connect(ui_->baselineButton,&QPushButton::clicked,this,[import]{import(true);});connect(ui_->importResultButton,&QPushButton::clicked,this,[import]{import(false);});
    connect(ui_->exportJsonButton,&QPushButton::clicked,this,[this]{save(false);});connect(ui_->exportCsvButton,&QPushButton::clicked,this,[this]{save(true);});
    connect(ui_->reportButton,&QPushButton::clicked,this,[this]{
        auto rows=ui_->resultsTable->selectionModel()->selectedRows();if(rows.isEmpty()){message("En az bir sonuç seçin.");return;}
        auto path=QFileDialog::getSaveFileName(this,"HTML rapor", "AeroCore_Test_Report.html","HTML (*.html)");if(path.isEmpty())return;
        QString html="<!doctype html><html lang='tr'><meta charset='utf-8'><title>AeroCore Test Raporu</title><style>body{font:15px system-ui;color:#17365D;max-width:1100px;margin:32px auto}table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccd6e3;padding:8px}h1{background:#17365D;color:white;padding:20px}svg{background:#f4f7fb;width:100%;height:auto}</style><h1>AeroCore • Test Raporu</h1><p>Gerçek PX4 SITL telemetrisinden ölçümler. Simülasyon sonuçları fiziksel uçuş doğrulaması yerine geçmez. Süreler host monotonic clock ile ölçülür; batarya enerjisi model kaynağına bağlıdır.</p>";
        for(auto index:rows){auto&r=results_[index.row()];html+="<h2>"+r.spec.name.toHtmlEscaped()+" / SYS "+QString::number(r.metrics["systemId"].toInt())+" — "+r.status.toHtmlEscaped()+"</h2><p>"+r.reason.toHtmlEscaped()+"</p><p>Etiket: "+r.spec.versionTag.toHtmlEscaped()+"<br>Başlangıç: "+r.started.toHtmlEscaped()+"<br>PX4 binary: "+r.firmware.toHtmlEscaped()+"<br>Model SHA256: "+r.profileHash.toHtmlEscaped()+"<br>Batarya: "+r.spec.batteryModel.toHtmlEscaped()+"</p><table><tr><th>Ölçüm</th><th>Değer</th></tr>";
            for(auto it=r.metrics.begin();it!=r.metrics.end();++it)html+="<tr><td>"+it.key().toHtmlEscaped()+"</td><td>"+(it.value().isDouble()?QString::number(it.value().toDouble(),'g',8):it.value().toString().toHtmlEscaped())+"</td></tr>";html+="</table>";
            double maxT=1,maxE=1;for(auto s:r.samples){maxT=std::max(maxT,s.t);maxE=std::max(maxE,s.error);}html+="<h3>Yatay hata (m) / zaman (sn)</h3><svg viewBox='0 0 900 250' xmlns='http://www.w3.org/2000/svg'><path d='M40 20 V220 H880' fill='none' stroke='#64748b'/><path d='";bool fresh=false;for(auto s:r.samples){if(!s.measured){fresh=false;continue;}html+=(fresh?" L":" M")+QString::number(40+s.t/maxT*840,'f',2)+" "+QString::number(220-s.error/maxE*200,'f',2);fresh=true;}html+="' stroke='#2563eb' fill='none'/><text x='42' y='245'>0</text><text x='780' y='245'>"+QString::number(maxT,'f',1)+" sn</text><text x='45' y='15'>"+QString::number(maxE,'f',2)+" m</text></svg><h3>Olay kayıtları</h3><pre>"+QString::fromUtf8(QJsonDocument(r.eventLog).toJson()).toHtmlEscaped()+"</pre><details><summary>Senaryo</summary><pre>"+QString::fromUtf8(QJsonDocument(r.spec.json()).toJson()).toHtmlEscaped()+"</pre></details>";
        }html+="</html>";QString error;if(!writeFile(path,html.toUtf8(),error))message(error);
    });
    // Bounded history loaded from durable per-run files; no fake/demo result rows.
    QDir history(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/experiments");auto files=history.entryList({"*.json"},QDir::Files,QDir::Time);for(auto file:files.mid(0,30)){QString error;auto o=readJson(history.filePath(file),error);if(o)if(auto r=LabResult::fromJson(*o,error))results_.prepend(*r);}refreshResults();
}
LabPanel::~LabPanel()=default;
void LabPanel::message(const QString&s){QMessageBox::warning(this,"AeroCore test laboratuvarı",s);emit log(s);}
void LabPanel::fill(const LabSpec&s){
    ui_->nameEdit->setText(s.name);ui_->versionEdit->setText(s.versionTag);ui_->kindCombo->setCurrentIndex(ui_->kindCombo->findData(s.kind));
#define SET(name,field) ui_->name->setValue(s.field)
    SET(altitudeSpin,altitude);SET(durationSpin,duration);SET(speedSpin,speed);SET(dwellSpin,dwell);SET(timeoutSpin,maxTime);SET(errorSpin,maxError);SET(altErrorSpin,maxAltError);SET(tiltSpin,maxTilt);ui_->coverageSpin->setValue(s.minCoverage*100);SET(windSpin,windSpeed);SET(directionSpin,windDirection);SET(payloadSpin,payload);SET(cgXSpin,cgX);SET(cgYSpin,cgY);SET(cgZSpin,cgZ);SET(gpsNoiseSpin,gpsNoise);SET(drainSpin,batteryDrain);SET(batteryMinSpin,batteryMin);
#undef SET
    ui_->batteryModelEdit->setText(s.batteryModel);ui_->energyCheck->setChecked(s.energyRequired);ui_->modeCombo->setCurrentIndex(ui_->modeCombo->findData(s.expectedMode));
    ui_->routeTable->blockSignals(true);ui_->routeTable->setRowCount(0);for(auto p:s.route)addRow(ui_->routeTable,{QString::number(p.east),QString::number(p.north)});ui_->routeTable->blockSignals(false);ui_->eventsTable->setRowCount(0);for(auto e:s.events)addRow(ui_->eventsTable,{QString::number(e.at),QString::number(e.duration),e.type,QString::number(e.a),QString::number(e.b)});ui_->paramTable->setRowCount(0);for(auto it=s.parameters.begin();it!=s.parameters.end();++it)addRow(ui_->paramTable,{it.key(),QString::number(it.value())});publishRoute();
}
LabSpec LabPanel::spec(QString&error)const{
    LabSpec s;s.name=ui_->nameEdit->text().trimmed();s.kind=ui_->kindCombo->currentData().toString();s.versionTag=ui_->versionEdit->text().trimmed();
#define GET(name,field) s.field=ui_->name->value()
    GET(altitudeSpin,altitude);GET(durationSpin,duration);GET(speedSpin,speed);GET(dwellSpin,dwell);GET(timeoutSpin,maxTime);GET(errorSpin,maxError);GET(altErrorSpin,maxAltError);GET(tiltSpin,maxTilt);s.minCoverage=ui_->coverageSpin->value()/100.;GET(windSpin,windSpeed);GET(directionSpin,windDirection);GET(payloadSpin,payload);GET(cgXSpin,cgX);GET(cgYSpin,cgY);GET(cgZSpin,cgZ);GET(gpsNoiseSpin,gpsNoise);GET(drainSpin,batteryDrain);GET(batteryMinSpin,batteryMin);
#undef GET
    s.batteryModel=ui_->batteryModelEdit->text().trimmed();s.energyRequired=ui_->energyCheck->isChecked();s.expectedMode=ui_->modeCombo->currentData().toString();
    auto cell=[&](QTableWidget*t,int r,int c){return t->item(r,c)?t->item(r,c)->text().trimmed():QString{};};
    auto value=[&](QTableWidget*t,int r,int c){bool ok=false;double n=cell(t,r,c).toDouble(&ok);if(!ok||!std::isfinite(n))error="Tabloda geçersiz sayı: satır "+QString::number(r+1);return n;};
    for(int r=0;r<ui_->routeTable->rowCount();++r)s.route.append({value(ui_->routeTable,r,0),value(ui_->routeTable,r,1)});
    for(int r=0;r<ui_->eventsTable->rowCount();++r)s.events.append({value(ui_->eventsTable,r,0),value(ui_->eventsTable,r,1),value(ui_->eventsTable,r,3),value(ui_->eventsTable,r,4),cell(ui_->eventsTable,r,2)});
    for(int r=0;r<ui_->paramTable->rowCount();++r){auto k=cell(ui_->paramTable,r,0);if(s.parameters.contains(k))error="Tekrarlanan parametre: "+k;s.parameters[k]=value(ui_->paramTable,r,1);}
    if(error.isEmpty())error=s.validate();return s;
}
void LabPanel::start(bool batch){
    if(comparing_)return;
    if(externalCommandPending&&externalCommandPending()){message("Ana panelde bekleyen uçuş komutunun sonucunu bekleyin.");return;}
    QString error;auto s=spec(error);if(!error.isEmpty()){message(error);return;}applyVehicle(s);if(s.kind=="regression"){
        if(!baseline_||comparable(s)!=comparable(baseline_->spec)){message("Regresyon için aynı koşullara sahip PASS referans sonucu gerekli. Ad, tür ve sürüm etiketi karşılaştırma dışında tutulur.");return;}
        if(baseline_->profileHash!=session_->profileHash()||baseline_->firmware==session_->firmware()){message("Regresyon için aynı model profili ve farklı doğrulanmış PX4 binary SHA256 gerekir. Yeni yazılımı derleyip simülasyonu yeniden başlatın.");return;}}
    QVector<LabSpec> queue;
    if(batch){auto values=[&](QLineEdit*field,double lo,double hi){QVector<double> out;for(auto token:field->text().split(',')){bool ok=false;double n=token.trimmed().toDouble(&ok);if(!ok||!std::isfinite(n)||n<lo||n>hi){error="Toplu deney listeleri sınırlar içinde virgülle ayrılmış sayılar olmalı.";return QVector<double>{};}out.append(n);}return out;};auto speeds=values(ui_->speedsEdit,.2,10),winds=values(ui_->windsEdit,0,15);if(!error.isEmpty()){message(error);return;}if(speeds.size()*winds.size()*ui_->repeatSpin->value()>100){message("Toplu deney en fazla 100 uçuş olabilir.");return;}
        if(s.kind=="regression"&&(speeds.size()!=1||winds.size()!=1||speeds[0]!=s.speed||winds[0]!=s.windSpeed)){message("Regresyonda referans koşulları değiştirilemez.");return;}
        for(double speed:speeds)for(double wind:winds)for(int n=0;n<ui_->repeatSpin->value();++n){auto trial=s;trial.speed=speed;trial.windSpeed=wind;trial.name=s.name+QString(" / v%1 w%2 #%3").arg(speed).arg(wind).arg(n+1);queue.append(trial);}}
    else queue.append(s);
    publishRoute();runSpec_=s;regressionLimit_=ui_->regressionSpin->value();if(!runner_->start(queue,error)){message(error);return;}ui_->tabs->setCurrentWidget(ui_->resultsTab);
}
void LabPanel::result(LabResult r){
    if(r.spec.kind=="regression"&&baseline_){if(r.status=="PASS"&&r.metrics.contains("meanErrorM")&&baseline_->metrics.contains("meanErrorM")){double reference=baseline_->metrics["meanErrorM"].toDouble(),current=r.metrics["meanErrorM"].toDouble();if(reference>1e-6){double delta=(current-reference)/reference*100;r.metrics["regressionErrorIncreasePct"]=delta;if(delta>regressionLimit_){r.status="FAIL";r.reason="Ortalama rota hatası referansa göre izin verilen artışı aştı.";}r.metrics["regressionLimitPct"]=regressionLimit_;r.metrics["baselineId"]=baseline_->id;}else{r.status="INCONCLUSIVE";r.reason="Referans ortalama hata sıfıra çok yakın; yüzde karşılaştırması tanımsız.";}}}
    if(comparing_)comparisonIds_.append(r.id);
    if(results_.size()>=200)results_.removeFirst();results_.append(r);QDir d(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/experiments");if(!d.exists())QDir().mkpath(d.absolutePath());QString error;if(!writeFile(d.filePath(r.id+".json"),QJsonDocument(r.json()).toJson(QJsonDocument::Compact),error))emit log("Sonuç otomatik kaydedilemedi: "+error);QString html="<!doctype html><html lang='tr'><meta charset='utf-8'><title>AeroCore deney raporu</title><style>body{font:15px system-ui;max-width:1050px;margin:32px auto;color:#17365D}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#F3F6FA;padding:16px}svg{background:#F3F6FA;width:100%}</style><h1>"+r.spec.name.toHtmlEscaped()+"</h1><h2>"+r.status.toHtmlEscaped()+"</h2><p>"+r.reason.toHtmlEscaped()+"</p><p>SYS "+QString::number(r.metrics["systemId"].toInt())+" • "+r.metrics["vehicleName"].toString().toHtmlEscaped()+"<br>"+r.started.toHtmlEscaped()+"<br>PX4 SHA256: "+r.firmware.toHtmlEscaped()+"<br>Fizik profili SHA256: "+r.profileHash.toHtmlEscaped()+"</p>";
    double maxT=1,maxE=1;for(auto sample:r.samples){maxT=std::max(maxT,sample.t);maxE=std::max(maxE,sample.error);}html+="<h3>Yatay rota hatası / zaman</h3><svg viewBox='0 0 900 250' xmlns='http://www.w3.org/2000/svg'><path d='M40 20 V220 H880' fill='none' stroke='#64748B'/><path d='";bool fresh=false;for(auto sample:r.samples){if(!sample.measured){fresh=false;continue;}html+=(fresh?" L":" M")+QString::number(40+sample.t/maxT*840,'f',2)+" "+QString::number(220-sample.error/maxE*200,'f',2);fresh=true;}html+="' stroke='#2563EB' fill='none'/><text x='45' y='15'>"+QString::number(maxE,'f',2)+" m</text><text x='780' y='245'>"+QString::number(maxT,'f',1)+" sn</text></svg>";
    for(auto pair:{qMakePair(QString("Metrikler"),QString::fromUtf8(QJsonDocument(r.metrics).toJson())),qMakePair(QString("Koşullar ve geçme/kalma sınırları"),QString::fromUtf8(QJsonDocument(r.spec.json()).toJson())),qMakePair(QString("Olay kanıtları"),QString::fromUtf8(QJsonDocument(r.eventLog).toJson()))})html+="<h3>"+pair.first+"</h3><pre>"+pair.second.toHtmlEscaped()+"</pre>";
    html+="<p>PX4/Gazebo simülasyon ölçümleri. Batarya Wh değeri yalnız telemetri integralidir; fiziksel enerji modeli doğrulanmış değildir.</p></html>";
    if(!writeFile(d.filePath(r.id+".html"),html.toUtf8(),error))emit log("Otomatik HTML raporu yazılamadı: "+error);
    refreshResults();ui_->resultsTable->selectRow(results_.size()-1);emit log("Test sonucu: "+r.status+" / "+r.reason);
}
void LabPanel::refreshResults(){ui_->resultsTable->setRowCount(0);for(auto&r:results_)addRow(ui_->resultsTable,{r.spec.name,r.spec.versionTag,r.status,QString::number(r.spec.speed),QString::number(r.spec.windSpeed),number(r.metrics,"meanErrorM"),number(r.metrics,"maxErrorM"),r.metrics.contains("coverage")?QString::number(r.metrics["coverage"].toDouble()*100,'f',1):"—",number(r.metrics,"durationS")});selection();}
void LabPanel::selection(){QVector<LabResult> selected;QString text; aero::Stats meanErrors,durations;int passes=0;for(auto i:ui_->resultsTable->selectionModel()->selectedRows()){auto&r=results_[i.row()];if(selected.size()<4)selected.append(r);if(r.metrics.contains("meanErrorM"))meanErrors.add(r.metrics["meanErrorM"].toDouble());if(r.metrics.contains("durationS"))durations.add(r.metrics["durationS"].toDouble());passes+=r.status=="PASS";text+=r.spec.name+" / SYS "+QString::number(r.metrics["systemId"].toInt())+" / "+r.status+"\n"+r.reason+"\nPX4: "+r.firmware+"\nModel: "+r.profileHash+"\nBatarya: "+r.spec.batteryModel+"\n"+QString::fromUtf8(QJsonDocument(r.metrics).toJson())+QString::fromUtf8(QJsonDocument(r.eventLog).toJson())+"\n";}
    chart_->setResults(selected);ui_->detailsEdit->setPlainText(text);int count=ui_->resultsTable->selectionModel()->selectedRows().size();QString summary=QString("Seçili %1 • PASS %2 • İlk dört araç/deney grafikte").arg(count).arg(passes);if(meanErrors.count){summary+=QString(" • Ortalama hata %1 m").arg(meanErrors.mean,0,'f',3);if(auto sd=meanErrors.deviation())summary+=QString(" ± %1 (deneyler arası σ)").arg(*sd,0,'f',3);}ui_->summaryLabel->setText(summary+" • Farklı koşullar birlikte seçilmiş olabilir.");}
void LabPanel::save(bool csv){auto rows=ui_->resultsTable->selectionModel()->selectedRows();if(rows.size()!=1){message("Dışa aktarım için bir sonuç seçin; çoklu karşılaştırma için HTML raporu kullanın.");return;}const auto&r=results_[rows[0].row()];auto path=QFileDialog::getSaveFileName(this,"Sonucu dışa aktar",r.id+(csv?".csv":".json"),csv?"CSV (*.csv)":"JSON (*.json)");if(path.isEmpty())return;QByteArray bytes;
    if(csv){QString data="run_id,scenario,version,status,time_host_s,time_boot_s,east_m,north_m,relative_alt_m,horizontal_error_m,altitude_error_m,roll_deg,pitch_deg,yaw_deg,custom_mode,battery_pct,fresh\n";for(auto s:r.samples)data+=csvQuote(r.id)+","+csvQuote(r.spec.name)+","+csvQuote(r.spec.versionTag)+","+csvQuote(r.status)+QString(",%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13\n").arg(s.t,0,'f',4).arg(s.boot,0,'f',4).arg(s.east,0,'f',4).arg(s.north,0,'f',4).arg(s.alt,0,'f',4).arg(s.error,0,'f',4).arg(s.altError,0,'f',4).arg(s.roll,0,'f',3).arg(s.pitch,0,'f',3).arg(s.yaw,0,'f',3).arg(s.mode).arg(s.battery).arg(s.measured?1:0);bytes=data.toUtf8();}else bytes=QJsonDocument(r.json()).toJson();QString error;if(!writeFile(path,bytes,error))message(error);
}
void LabPanel::tick(double now){transport_->tick(now);runner_->tick(now);bool running=busy();for(auto*w:{ui_->scenarioTab,ui_->routeTab,ui_->eventsTab,ui_->batchTab})w->setEnabled(!running);ui_->runButton->setEnabled(!running);ui_->batchButton->setEnabled(!running);ui_->abortButton->setEnabled(running);ui_->progressLabel->setText(runner_->phaseText()+(running?" • "+runner_->current().spec.name:""));}
void LabPanel::sessionEnded(){comparisonCancelled_=true;comparing_=false;comparisonQueue_.clear();if(runner_->busy())runner_->tick(sitl::monoSeconds());transport_->cancel();}
void LabPanel::addMapPoint(double lat,double lon){if(busy())return;auto v=state_->vehicles.constFind(state_->selected);if(v==state_->vehicles.cend()||sitl::monoSeconds()-v->posAt>=1){emit log("Rota eklemek için taze konum gerekli.");return;}auto q=aero::local(lat,lon,v->lat,v->lon);if(std::abs(q.east)>500||std::abs(q.north)>500){emit log("Rota noktası ±500 m sınırı dışında.");return;}addRow(ui_->routeTable,{QString::number(q.east,'f',2),QString::number(q.north,'f',2)});publishRoute();ui_->tabs->setCurrentWidget(ui_->routeTab);}
void LabPanel::publishRoute(){QString error;auto s=spec(error);if(!error.isEmpty())return;auto v=state_->vehicles.constFind(state_->selected);if(v==state_->vehicles.cend()||v->posAt==0)return;QVector<QPointF> route;route.append({v->lon,v->lat});for(auto p:s.route){auto g=aero::geographic(p,v->lat,v->lon);route.append({g.east,g.north});}emit routeChanged(route);}

void LabPanel::applyVehicle(LabSpec&s)const{QString error;auto o=session_->vehicleProfile(state_->selected);if(auto v=VehicleProfile::fromJson(o,error))v->apply(s);}
void LabPanel::selectedVehicleChanged(){if(runner_->busy())return;session_->setActiveSystem(state_->selected);QString error;auto s=spec(error);if(error.isEmpty()){applyVehicle(s);ui_->payloadSpin->setValue(s.payload);ui_->cgXSpin->setValue(s.cgX);ui_->cgYSpin->setValue(s.cgY);ui_->cgZSpin->setValue(s.cgZ);ui_->gpsNoiseSpin->setValue(s.gpsNoise);ui_->drainSpin->setValue(s.batteryDrain);ui_->batteryMinSpin->setValue(s.batteryMin);ui_->batteryModelEdit->setText(s.batteryModel);ui_->vehicleLabel->setText(QString("SYS %1 / %2 / Gazebo x500").arg(state_->selected).arg(session_->vehicleProfile(state_->selected)["name"].toString()));}}
void LabPanel::compareFleet(){
    if(busy()||(externalCommandPending&&externalCommandPending())){message("Çalışan deney / komutun bitmesini bekleyin.");return;}
    QString error;auto s=spec(error);if(!error.isEmpty()){message(error);return;}
    if(!session_->owned()||!session_->running()){message("Önce çoklu drone oturumunu başlatın.");return;}
    if(s.kind=="regression"){message("Çoklu araç karşılaştırması için regresyon yerine uçuş testi seçin.");return;}
    const double now=sitl::monoSeconds();comparisonQueue_.clear();
    for(int sys=1;sys<=session_->vehicleCount();++sys){auto i=state_->vehicles.constFind(sys);if(i==state_->vehicles.cend()||i->armed||i->landed!=1||now-i->landedAt>=3||now-i->heartbeatAt>=3||now-i->posAt>=1){message("Tüm araçlar bağlı, yerde ve DISARMED olmalı; eksik SYS "+QString::number(sys));comparisonQueue_.clear();return;}comparisonQueue_.append(sys);}
    comparisonIds_.clear();comparisonSystem_=0;comparisonSpec_=s;comparing_=true;comparisonCancelled_=false;nextComparison();
}
void LabPanel::nextComparison(){
    if(!comparing_)return;
    if(comparisonQueue_.isEmpty()){comparing_=false;ui_->resultsTable->clearSelection();for(int row=0;row<results_.size();++row)if(comparisonIds_.contains(results_[row].id))ui_->resultsTable->selectionModel()->select(ui_->resultsTable->model()->index(row,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);selection();emit log("Araç karşılaştırması tamamlandı. Sonuçlarda birden fazla satır seçip grafik ve HTML raporu açın.");return;}
    // An inconclusive cleanup must not be followed by another vehicle's flight.
    if(comparisonSystem_>0){auto v=state_->vehicles.constFind(comparisonSystem_);double now=sitl::monoSeconds();if(v==state_->vehicles.cend()||v->armed||v->landed!=1||now-v->landedAt>=3){comparing_=false;comparisonQueue_.clear();message("Önceki aracın inişi doğrulanamadı; karşılaştırma durdu.");return;}}
    state_->selected=comparisonQueue_.takeFirst();comparisonSystem_=state_->selected;session_->setActiveSystem(state_->selected);
    auto s=comparisonSpec_;applyVehicle(s);s.name+=QString(" / SYS %1 — %2").arg(state_->selected).arg(session_->vehicleProfile(state_->selected)["name"].toString());selectedVehicleChanged();publishRoute();QString error;
    if(!runner_->start({s},error)){comparing_=false;comparisonQueue_.clear();message(error);return;}ui_->tabs->setCurrentWidget(ui_->resultsTab);
}
