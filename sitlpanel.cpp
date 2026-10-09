#include "sitlpanel.h"
#include "ui_sitlpanel.h"
#include "FlightViews.hpp"
#include "SimulatorSession.hpp"
#include "SimulationProtocol.hpp"
#include "LabPanel.hpp"
#include "FleetPanel.hpp"
#include "SessionRecording.hpp"
#include "Scene3DView.hpp"
#include "CameraPanel.hpp"
#include "EnvironmentDiscovery.hpp"
#include <QTabWidget>
#include <QFrame>
#include <QStandardPaths>
#include <QDir>
#include <QSignalBlocker>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTimer>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QSettings>
#include <QFileDialog>
#include <QSaveFile>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QTextCursor>
#include <array>
#include <limits>
#include <cmath>

namespace {
struct ControlState {
    std::uint8_t sequence=0;
    std::uint16_t pending=0;
    int pendingSystem=0;
    double sentAt=0,heartbeatAt=0;
    bool configured=false;
    QString failure;
};
QString value(double n,bool fresh,int digits=2){return fresh?QString::number(n,'f',digits):QStringLiteral("—");}
QString resultName(int code){
    switch(code){case 0:return "KABUL EDİLDİ";case 1:return "GEÇİCİ OLARAK REDDEDİLDİ";
    case 2:return "REDDEDİLDİ";case 3:return "DESTEKLENMİYOR";case 4:return "BAŞARISIZ";
    case 5:return "İŞLENİYOR";case 6:return "İPTAL EDİLDİ";default:return QString("SONUÇ %1").arg(code);}
}
}
SitlPanel::SitlPanel(QWidget *parent):QWidget(parent){
    auto recording=std::make_shared<SessionRecording>();
    auto state=std::make_shared<FlightState>();auto control=std::make_shared<ControlState>();
    auto *session=new SimulatorSession(this);auto *socket=new QUdpSocket(this);
    auto *timer=new QTimer(this);timer->setInterval(50);timer->start();
    Ui::SitlPanel ui;ui.setupUi(this);
    auto*fleet=new FleetPanel(this);fleet->bindState(state);auto*replay=new ReplayPanel(this);
    ui.workspaceTabs->addTab(fleet,"Araç profilleri / çoklu drone");ui.workspaceTabs->addTab(replay,"Kayıt oynatma");
    auto*scenePage=new QWidget(this);scenePage->setObjectName("liveScenePage");auto*sceneLayout=new QVBoxLayout(scenePage);
    auto*sceneBar=new QHBoxLayout;auto*sceneLegend=new QLabel("● SYS 1 PEMBE     ● SYS 2 KIRMIZI     ● SYS 3 SARI     •     Grid: telemetri için 3B referans düzlemi");sceneLegend->setStyleSheet("color:#17365D;font-weight:600;padding:8px;");sceneBar->addWidget(sceneLegend,1);
    auto*follow3D=new QCheckBox("Seçili drone'u izle");follow3D->setChecked(true);auto*reset3D=new QPushButton("Görünümü sıfırla");sceneBar->addWidget(follow3D);sceneBar->addWidget(reset3D);sceneLayout->addLayout(sceneBar);
    auto*sceneSplit=new QSplitter(Qt::Horizontal,scenePage);auto*scene=new Scene3DView(state);auto*sceneCamera=new CameraPanel(scenePage);sceneSplit->addWidget(scene);sceneSplit->addWidget(sceneCamera);sceneSplit->setStretchFactor(0,3);sceneSplit->setStretchFactor(1,2);sceneLayout->addWidget(sceneSplit,1);
    ui.workspaceTabs->insertTab(1,scenePage,"3B GÖREV SAHNESİ");
    connect(follow3D,&QCheckBox::toggled,scene,&Scene3DView::setFollowing);
    connect(reset3D,&QPushButton::clicked,scene,&Scene3DView::resetView);
    auto *distribution=ui.distributionEdit;auto *repository=ui.repositoryEdit;distribution->setMaximumWidth(180);ui.networkLabel->setMaximumWidth(260);ui.systemSpin->setMaximumWidth(90);ui.altitudeSpin->setMaximumWidth(140);
    QSettings settings;const bool firstSetup=!settings.contains("wsl/distribution")||!settings.contains("wsl/repository");
    distribution->setText(settings.value("wsl/distribution").toString());
    repository->setText(settings.value("wsl/repository").toString());
    distribution->setPlaceholderText("WSL dağıtımı taranıyor…");
    repository->setPlaceholderText("WSL içindeki PX4 kaynak klasörü");
    auto *discover=new EnvironmentDiscovery(this);
    auto *discoverButton=new QPushButton("WSL / PX4 bul",this);
    discoverButton->setObjectName("environmentDiscoveryButton");
    ui.optionsLayout->addWidget(discoverButton);
    auto *start=ui.startButton;auto *stop=ui.stopButton;auto *network=ui.networkLabel;
    auto *status=ui.statusLabel;auto *commandStatus=ui.commandLabel;
    auto *selected=ui.systemSpin;auto *altitude=ui.altitudeSpin;
    auto *arm=ui.armButton;auto *takeoff=ui.takeoffButton;auto *land=ui.landButton;auto *disarm=ui.disarmButton;
    auto *online=ui.onlineCheck;auto *follow=ui.followButton;
    auto *map=new MapView(state);map->setOnline(false);ui.mapHostLayout->setContentsMargins(0,0,0,0);ui.mapHostLayout->addWidget(map);
    auto *drone=new DroneView(state);ui.droneHostLayout->setContentsMargins(0,0,0,0);ui.droneHostLayout->addWidget(drone);
    ui.viewsSplitter->setStretchFactor(0,3);ui.viewsSplitter->setStretchFactor(1,2);
    auto *table=ui.telemetryTable;table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setMaximumHeight(125);
    auto *logs=ui.logEdit;logs->setMaximumHeight(170);auto *saveLogs=ui.exportButton;
    auto *transport=new LabTransport(state,session,socket,this);
    auto *lab=new LabPanel(state,session,transport,this);ui.labHostLayout->addWidget(lab);lab->externalCommandPending=[control]{return control->pending!=0;};
    connect(lab,&LabPanel::routeChanged,map,&MapView::setPlan);
    connect(lab,&LabPanel::routeChanged,scene,&Scene3DView::setPlan);
    connect(lab,&LabPanel::routeChanged,this,[recording](const QVector<QPointF>&p){QJsonArray route;for(auto q:p)route.append(QJsonArray{q.x(),q.y()});recording->updateMetadata({{"routePlan",route}});});map->pointAdded=[lab](double lat,double lon){lab->addMapPoint(lat,lon);};
    const auto log=[logs,recording](const QString &text){recording->event(text,sitl::monoSeconds());logs->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss")+"  "+text.trimmed());logs->moveCursor(QTextCursor::End);};
    auto manualScan=std::make_shared<bool>(false);
    connect(discoverButton,&QPushButton::clicked,this,[discover,manualScan]{*manualScan=true;discover->start();});
    connect(discover,&EnvironmentDiscovery::busyChanged,this,[=](bool busy){
        discoverButton->setEnabled(!busy&&!session->active());
        if(!session->active())start->setEnabled(!busy);
        if(busy){status->setStyleSheet("");status->setText("Kurulu WSL ve PX4 kaynağı aranıyor…");}
        else if(!session->active())status->setText("Kurulum taraması tamamlandı; alanları kontrol edip simülasyonu başlatın.");
    });
    connect(discover,&EnvironmentDiscovery::message,this,[=](const QString &text){
        log(text);status->setText("KURULUM: "+text);
        status->setStyleSheet("background:#FEF3C7;color:#78350F;padding:8px;");
    });
    connect(discover,&EnvironmentDiscovery::found,this,[=](const QString &distro,const QString &path){
        if(*manualScan||distribution->text().trimmed().isEmpty())distribution->setText(distro);
        if(*manualScan||repository->text().trimmed().isEmpty())repository->setText(path);
        QSettings settings;settings.setValue("wsl/distribution",distribution->text().trimmed());
        settings.setValue("wsl/repository",repository->text().trimmed());
        log(QString("WSL/PX4 bulundu: %1 • %2").arg(distribution->text(),repository->text()));
    });
    if(firstSetup)QTimer::singleShot(0,discover,&EnvironmentDiscovery::start);
    connect(lab,&LabPanel::log,this,log);
    connect(fleet,&FleetPanel::compareRequested,lab,&LabPanel::compareFleet);
    connect(lab,&LabPanel::replayResult,this,[=](const LabResult&r){replay->openResult(r);ui.workspaceTabs->setCurrentWidget(replay);});
    auto*recordButton=new QPushButton("Kaydı dosyaya tamamla",this);recordButton->setObjectName("finishRecordingButton");ui.optionsLayout->addWidget(recordButton);
    connect(recordButton,&QPushButton::clicked,this,[=]{if(!recording->recording()){log("Oturum kaydı açılışta otomatik başlar; mevcut kayıt tamamlandı.");return;}QString e;if(!recording->save(e))log("Kayıt yazılamadı: "+e);else log("Oturum kaydedildi: "+recording->path());});
    scene->selected=[selected=ui.systemSpin](int sys){if(!selected->isEnabled())return false;selected->setValue(sys);return true;};
    connect(online,&QCheckBox::toggled,map,&MapView::setOnline);connect(follow,&QPushButton::clicked,map,&MapView::followDrone);
    connect(selected,qOverload<int>(&QSpinBox::valueChanged),this,[state,map,control,log,session,lab,sceneCamera,scene](int n){
        state->selected=n;sceneCamera->setSystem(n);scene->update();session->setActiveSystem(n);lab->selectedVehicleChanged();map->followDrone();if(control->pending)log("Araç seçimi değişti; bekleyen komut eski SYS ID için izleniyor.");
    });
    connect(saveLogs,&QPushButton::clicked,this,[=,this]{
        const auto path=QFileDialog::getSaveFileName(this,"Simülasyon kayıtları","AeroCore_Simulator_Log.txt","Metin (*.txt)");
        if(path.isEmpty())return;QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)){QMessageBox::warning(this,"Kayıt",file.errorString());return;}
        file.write(logs->toPlainText().toUtf8());if(!file.commit())QMessageBox::warning(this,"Kayıt",file.errorString());
    });
    connect(session,&SimulatorSession::failure,this,[=](const QString &message){
        control->failure=message;status->setText("BAŞLATMA HATASI: "+message);
        status->setStyleSheet("background:#FEF2F2;color:#B91C1C;padding:10px;font-weight:bold;");
    });
    connect(session,&SimulatorSession::log,this,log);
    connect(session,&SimulatorSession::networkReady,this,[=](const QString &ip,const QString &gateway){
        state->peer=ip;network->setText(QString("WSL %1 → Windows %2 • UDP 14560").arg(ip,gateway));
    });
    connect(session,&SimulatorSession::ready,this,[=]{control->configured=true;QJsonArray manifests;for(int sys=1;sys<=session->vehicleCount();++sys)manifests.append(session->vehicleProfile(sys));recording->updateMetadata(QJsonObject{{"manifest",manifests},{"firmware",session->firmware()},{"world","aerocore_lab"}});lab->selectedVehicleChanged();control->failure.clear();status->setStyleSheet("");status->setText("PX4 akışı yapılandırıldı; heartbeat bekleniyor…");});
    connect(session,&SimulatorSession::ended,this,[=]{
        lab->sessionEnded();QString error;if(recording->recording()){if(recording->save(error))log("Oturum kaydedildi: "+recording->path());else log("Kayıt yazılamadı: "+error);}fleet->setActive(false);socket->close();state->vehicles.clear();state->peer.clear();control->configured=false;control->pending=0;
        start->setEnabled(!discover->busy());discoverButton->setEnabled(!discover->busy());stop->setEnabled(false);distribution->setEnabled(true);repository->setEnabled(true);if(control->failure.isEmpty())status->setText("Simülasyon oturumu kapalı.");commandStatus->setText("Komut: yok / oturum kapalı");
    });
    connect(start,&QPushButton::clicked,this,[=]{
        socket->close();
        if(!socket->bind(QHostAddress::AnyIPv4,14560)){log("UDP 14560 açılamadı: "+socket->errorString());return;}
        if(distribution->text().trimmed().isEmpty()||repository->text().trimmed().isEmpty()){socket->close();log("Dağıtım ve PX4 klasörü boş olamaz.");return;}
        QString specError;auto spec=lab->spec(specError);if(!specError.isEmpty()){socket->close();log("Test senaryosu geçersiz: "+specError);return;}
        QString fleetError;auto profiles=fleet->profiles(fleetError);if(!fleetError.isEmpty()){socket->close();log(fleetError);return;}
        session->setFleet(profiles);selected->setRange(1,profiles.size());session->setActiveSystem(selected->value());
        session->setLabProfile(spec.profile());fleet->setActive(true);
        QDir records(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/recordings");QDir().mkpath(records.path());
        recording->start(records.filePath(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz")+".json"),QJsonObject{{"vehicles",profiles},{"started",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"version","0.7.2"}});
        session->setConfiguration(distribution->text().trimmed(),repository->text().trimmed());
        QSettings settings;settings.setValue("wsl/distribution",distribution->text().trimmed());settings.setValue("wsl/repository",repository->text().trimmed());
        state->vehicles.clear();scene->resetOrigin();state->parser=sitl::MavlinkParser{};state->peer.clear();*control=ControlState{};
        status->setStyleSheet("");distribution->setEnabled(false);repository->setEnabled(false);discoverButton->setEnabled(false);
        start->setEnabled(false);stop->setEnabled(true);status->setText("WSL / Gazebo / PX4 başlatılıyor…");session->start();
    });
    connect(stop,&QPushButton::clicked,this,[=]{log("Panelin başlattığı simülasyon kapatılıyor…");session->stop();});
    const auto send=[=](std::uint16_t cmd,std::array<float,7> params){
        const double now=sitl::monoSeconds();auto it=state->vehicles.constFind(state->selected);
        if(lab->busy()||!session->owned()||!session->running()||!control->configured||control->pending||it==state->vehicles.cend()||!it->px4||now-it->heartbeatAt>=3){log("Komut gönderilmedi: canlı panel oturumu gerekli.");return;}
        const bool ground=now-it->landedAt<3&&it->landed==1;
        if(cmd==400&&(!ground||(params[0]==1&&it->armed))){log("ARM/DISARM için taze yerde bilgisi gerekli.");return;}
        if(cmd==22&&(!it->armed||!ground||now-it->posAt>=3)){log("Kalkış için ARMED, yerde ve taze konum bilgisi gerekli.");return;}
        if(cmd==21&&!it->armed){log("Araç ARMED değil.");return;}
        const auto bytes=sitl::commandLong(cmd,std::uint8_t(state->selected),1,params,control->sequence++);
        if(socket->writeDatagram(reinterpret_cast<const char*>(bytes.data()),qint64(bytes.size()),QHostAddress(state->peer),session->remotePort(state->selected))!=qint64(bytes.size())){log(socket->errorString());return;}
        control->pending=cmd;control->pendingSystem=state->selected;control->sentAt=now;
        commandStatus->setText(QString("SYS %1 / komut %2 gönderildi — PX4 onayı bekleniyor").arg(state->selected).arg(cmd));
    };
    connect(arm,&QPushButton::clicked,this,[send]{send(400,{1,0,0,0,0,0,0});});
    connect(disarm,&QPushButton::clicked,this,[send]{send(400,{0,0,0,0,0,0,0});});
    connect(takeoff,&QPushButton::clicked,this,[=]{
        auto it=state->vehicles.constFind(state->selected);if(it==state->vehicles.cend())return;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        // COMMAND_LONG NAV_TAKEOFF uses AMSL altitude: estimated home AMSL + requested AGL.
        send(22,{0,0,0,nan,nan,nan,float(it->amsl-it->alt+altitude->value())});
    });
    connect(land,&QPushButton::clicked,this,[send]{const float nan=std::numeric_limits<float>::quiet_NaN();send(21,{0,0,0,nan,nan,nan,nan});});
    connect(timer,&QTimer::timeout,this,[=]{
        const double now=sitl::monoSeconds();
        if(session->running()&&!state->peer.isEmpty()&&now-control->heartbeatAt>=1){
            const auto bytes=sitl::gcsHeartbeat(control->sequence++);
            transport->heartbeat(bytes);control->heartbeatAt=now;
        }
        auto datagrams=transport->drain(now);
        for(int n=0;n<64&&socket->state()==QAbstractSocket::BoundState&&socket->hasPendingDatagrams();++n){
            auto d=socket->receiveDatagram(4096);if(state->peer.isEmpty()||d.senderAddress()!=QHostAddress(state->peer)||(d.senderPort()<14561||d.senderPort()>=14561+session->vehicleCount()))continue;
            datagrams.append(transport->receive(d,now));
        }
        for(const auto &datagram:datagrams){
            state->parser.reset();
            for(char c:datagram.data())if(auto packet=state->parser.feed(static_cast<unsigned char>(c),now)){
                if(packet->system==0||packet->system>session->vehicleCount()||packet->component!=1)continue;
                if(datagram.senderPort()!=session->remotePort(packet->system))continue;
                if(!state->vehicles.contains(packet->system)&&state->vehicles.size()>=32)continue;
                auto &v=state->vehicles[packet->system];v.last=now;++v.packets;
                switch(packet->kind){
                case sitl::PacketKind::Heartbeat:v.heartbeatAt=now;v.armed=(packet->baseMode&128)!=0;v.customMode=packet->customMode;v.px4=packet->autopilot==12&&packet->vehicleType==2;break;
                case sitl::PacketKind::Position:
                    v.lat=packet->latitude;v.lon=packet->longitude;v.alt=packet->relativeAltitudeM;v.amsl=packet->altitudeM;v.posAt=now;v.bootMs=packet->bootMs;
                    if(v.trail.isEmpty()||std::hypot(v.trail.last().x()-v.lon,v.trail.last().y()-v.lat)>0.000001){v.trail.append(QPointF(v.lon,v.lat));if(v.trail.size()>600)v.trail.removeFirst();}break;
                case sitl::PacketKind::Attitude:v.roll=packet->roll*180/3.141592653589793;v.pitch=packet->pitch*180/3.141592653589793;v.yaw=packet->yaw*180/3.141592653589793;v.attitudeAt=now;break;
                case sitl::PacketKind::Battery:case sitl::PacketKind::System:v.battery=packet->remainingPercent;v.batteryAt=now;v.voltage=packet->voltageV;v.current=packet->currentA;v.powerAt=now;v.powerValid=packet->voltageValid&&packet->currentValid&&packet->currentA>=0;break;
                case sitl::PacketKind::ExtendedState:v.landed=packet->landedState;v.landedAt=now;break;
                case sitl::PacketKind::Actuators:v.actuators=packet->actuators;v.activeOutputs=packet->activeOutputs;v.actuatorsAt=now;break;
                case sitl::PacketKind::Parameter:break;
                case sitl::PacketKind::Gps:v.gpsAt=now;v.fixType=packet->fixType;break;
                case sitl::PacketKind::StatusText:if(!packet->text.empty())log(QString("PX4 SYS %1: ").arg(packet->system)+QString::fromStdString(packet->text));break;
                case sitl::PacketKind::CommandAck:
                    if(control->pending&&packet->system==control->pendingSystem&&packet->command==control->pending&&(packet->ackTargetSystem==0||packet->ackTargetSystem==255)&&(packet->ackTargetComponent==0||packet->ackTargetComponent==190)){
                        const QString message=QString("SYS %1 / komut %2: %3").arg(packet->system).arg(packet->command).arg(resultName(packet->result));commandStatus->setText(message);log(message);
                        if(packet->result==5)control->sentAt=now;else control->pending=0;
                    }break;
                }
                transport->feed(*packet);
            }
        }
        if(control->pending&&now-control->sentAt>10){commandStatus->setText("Komut onayı zaman aşımı — sonuç bilinmiyor; canlı uçuş durumunu kontrol edin.");log(commandStatus->text());control->pending=0;}
        table->setRowCount(state->vehicles.size());int rowIndex=0;
        bool warning=false;
        for(auto it=state->vehicles.cbegin();it!=state->vehicles.cend();++it,++rowIndex){
            bool live=now-it->heartbeatAt<3,pos=it->posAt>0&&now-it->posAt<3,att=it->attitudeAt>0&&now-it->attitudeAt<3,bat=it->batteryAt>0&&now-it->batteryAt<5&&it->battery>=0&&it->battery<=100;
            warning|=!live||(pos&&it->alt>120)||(bat&&it->battery<20);
            QString flight=live?(it->armed?"ARMED":"DISARMED"):"BAĞLANTI KESİLDİ";
            if(live&&now-it->landedAt<3)flight+=it->landed==1?" / YERDE":it->landed==2?" / HAVADA":" / GEÇİŞ";
            QStringList cells={QString::number(it.key()),flight,value(it->lat,pos,6),value(it->lon,pos,6),value(it->alt,pos),value(it->roll,att),value(it->pitch,att),value(it->yaw,att),bat?QString::number(it->battery):"—",QString::number(it->packets)};
            for(int col=0;col<cells.size();++col){if(!table->item(rowIndex,col))table->setItem(rowIndex,col,new QTableWidgetItem);table->item(rowIndex,col)->setText(cells[col]);}
        }
        auto vehicle=state->vehicles.constFind(state->selected);
        bool live=vehicle!=state->vehicles.cend()&&vehicle->heartbeatAt>0&&now-vehicle->heartbeatAt<3;
        bool can=!lab->busy()&&!transport->busy()&&live&&control->failure.isEmpty()&&vehicle->px4&&session->owned()&&session->running()&&control->configured&&!control->pending;
        bool ground=live&&vehicle->landedAt>0&&now-vehicle->landedAt<3&&vehicle->landed==1;
        arm->setEnabled(can&&ground&&!vehicle->armed);takeoff->setEnabled(can&&ground&&vehicle->armed&&now-vehicle->posAt<3);land->setEnabled(can&&vehicle->armed);disarm->setEnabled(can&&ground&&vehicle->armed);
        bool anyArmed=false;for(const auto &v:state->vehicles)anyArmed|=v.armed&&now-v.heartbeatAt<3;
        stop->setEnabled(session->active()&&!anyArmed&&!lab->busy());
        selected->setEnabled(!lab->busy());
        lab->tick(now);
        if(selected->value()!=state->selected){QSignalBlocker block(selected);selected->setValue(state->selected);sceneCamera->setSystem(state->selected);}
        if(recording->recording()&&!recording->capture(*state,now)){QString error;recording->save(error);log(error.isEmpty()?"Kayıt 18000 kare sınırına ulaştı ve tamamlandı.":"Kayıt hatası: "+error);}
        if(control->configured&&control->failure.isEmpty()){const auto &stats=state->parser.stats();status->setText(QString("%1 • Araç %2 • Geçerli paket %3 • CRC hatası %4").arg(state->vehicles.isEmpty()?"TELEMETRİ BEKLENİYOR":warning?"UYARI: bağlantı / batarya / irtifa":"CANLI SİMÜLASYON").arg(state->vehicles.size()).arg(stats.valid).arg(stats.badCrc));}
        map->refresh();drone->update();if(ui.workspaceTabs->currentWidget()==scenePage)scene->update();
    });
}
