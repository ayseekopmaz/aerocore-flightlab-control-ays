#include "SimulatorSession.hpp"
#include <QHostAddress>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
QString decodeOutput(QByteArray bytes){
    if(bytes.startsWith("\xFF\xFE"))bytes.remove(0,2);
    if(bytes.contains('\0')){
        QString text;for(qsizetype i=0;i+1<bytes.size();i+=2)
            text.append(QChar(uchar(bytes[i])|(ushort(uchar(bytes[i+1]))<<8)));
        return text;
    }
    return QString::fromUtf8(bytes);
}
QString wslProgram(){
#ifdef Q_OS_WIN
    const QString system=qEnvironmentVariable("SystemRoot","C:/Windows");
    for(const auto &path:{system+"/System32/wsl.exe",system+"/Sysnative/wsl.exe"})
        if(QFile::exists(path))return path;
#endif
    return QStandardPaths::findExecutable("wsl.exe");
}
}
void SimulatorSession::reportFailure(const QString &message){failure_=message;emit log(message);emit failure(message);}
SimulatorSession::SimulatorSession(QObject *parent):QObject(parent){
    #ifdef Q_OS_WIN
    const auto hideConsole=[](QProcess::CreateProcessArguments *args){args->flags|=CREATE_NO_WINDOW;};
    process_.setCreateProcessArgumentsModifier(hideConsole);
    discovery_.setCreateProcessArgumentsModifier(hideConsole);
    #endif
    process_.setProcessChannelMode(QProcess::MergedChannels);
    discovery_.setProcessChannelMode(QProcess::MergedChannels);
    queryTimeout_.setSingleShot(true);queryTimeout_.setInterval(30000);
    connect(&queryTimeout_,&QTimer::timeout,this,[this]{
        stage_=Stage::Idle;reportFailure("WSL sorgusu 30 saniyede yanıt vermedi.");discovery_.kill();emit ended();
    });
    timeout_.setSingleShot(true);timeout_.setInterval(180000);
    commands_.setInterval(180);
    connect(&commands_,&QTimer::timeout,this,[this]{
        if(!failure_.isEmpty()){commands_.stop();queue_.clear();return;}
        if(queue_.isEmpty()){commands_.stop();emit ready();return;}
        process_.write(queue_.takeFirst().toUtf8()+"\n");
    });
    connect(&timeout_,&QTimer::timeout,this,[this]{
        reportFailure("PX4 açılışı 180 saniyede tamamlanmadı. Derleme/Gazebo kayıtlarını kontrol edin; süreç devam ediyor olabilir.");
    });
    connect(&discovery_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){
        queryTimeout_.stop();if(stage_==Stage::Idle)return;stage_=Stage::Idle;reportFailure("WSL başlatılamadı: "+discovery_.errorString());emit ended();
    });
    connect(&discovery_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
        queryTimeout_.stop();const auto text=decodeOutput(discovery_.readAll()).trimmed();
        if(stage_==Stage::Idle)return;
        if(stage_==Stage::Check){
            if(code==0){stage_=Stage::Idle;reportFailure("PX4 zaten çalışıyor (PID: "+text+"). Eski simülasyon oturumunu kapatın ve tekrar başlatın.");emit ended();return;}
            if(code!=1||exit!=QProcess::NormalExit){stage_=Stage::Idle;reportFailure("WSL/PX4 kontrolü başarısız: "+text);emit ended();return;}
            query({"hostname","-I"},Stage::Host);return;
        }
        if(code!=0||exit!=QProcess::NormalExit){stage_=Stage::Idle;reportFailure("WSL ağ sorgusu başarısız: "+text);emit ended();return;}
        if(stage_==Stage::Host){
            host_.clear();
            for(const auto &token:text.split(QRegularExpression("\\s+"))){QHostAddress a(token);if(a.protocol()==QAbstractSocket::IPv4Protocol){host_=a.toString();break;}}
            if(host_.isEmpty()){stage_=Stage::Idle;reportFailure("WSL IPv4 adresi bulunamadı. Çıktı: "+text);emit ended();return;}
            query({"ip","route","show","default"},Stage::Route);return;
        }
        const auto match=QRegularExpression("\\bvia\\s+([0-9.]+)").match(text);
        QHostAddress a(match.captured(1));
        if(!match.hasMatch()||a.protocol()!=QAbstractSocket::IPv4Protocol){stage_=Stage::Idle;reportFailure("WSL NAT ağ geçidi bulunamadı. Çıktı: "+text);emit ended();return;}
        gateway_=a.toString();stage_=Stage::Idle;emit networkReady(host_,gateway_);launch();
    });
    connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{
        QString chunk=decodeOutput(process_.readAllStandardOutput());
        chunk.remove(QRegularExpression("\\x1B\\[[0-?]*[ -/]*[@-~]"));chunk.remove('\r');emit log(chunk);
        output_=(output_+chunk).right(12000);
        auto fleetManifest=QRegularExpression("AERO_FLEET: ([^\\r\\n]+)[\\r\\n]").match(output_);
        if(fleetManifest.hasMatch()){
            auto m=QJsonDocument::fromJson(fleetManifest.captured(1).toUtf8()).object();partition_=m["partition"].toString();
            for(auto item:m["vehicles"].toArray()){auto v=item.toObject();profiles_[v["system"].toInt()]=v;}
        }
        if(!fleet_.isEmpty()&&!configured_&&output_.contains("AERO_FLEET_READY")){
            configured_=true;timeout_.stop();emit ready();
        }
        auto manifest=QRegularExpression("AERO_PROFILE: ([^\\r\\n]+)[\\r\\n]").match(output_);
        if(manifest.hasMatch()){auto m=QJsonDocument::fromJson(manifest.captured(1).toUtf8()).object();profileHash_=m["sha256"].toString();partition_=m["partition"].toString();}
        auto firmware=QRegularExpression("AERO_FIRMWARE: ([^\\r\\n]+)[\\r\\n]").match(output_);
        if(firmware.hasMatch())firmware_=firmware.captured(1);
        const auto launcherError=QRegularExpression("AERO_ERROR: ([^\r\n]+)[\r\n]").match(output_);
        if(launcherError.hasMatch()&&failure_.isEmpty())reportFailure(launcherError.captured(1));
        if(configured_)configurationOutput_=(configurationOutput_+chunk).right(12000);
        const auto mavlinkError=QRegularExpression("(?:ERROR\\s+\\[mavlink\\]|mavlink for network on port 14561 is not running)[^\r\n]*[\r\n]").match(configurationOutput_);
        if(configured_&&mavlinkError.hasMatch()){
            commands_.stop();queue_.clear();
            if(failure_.isEmpty())reportFailure("MAVLink hattı kurulamadı: "+mavlinkError.captured().trimmed());
        }
        if(fleet_.isEmpty()&&!configured_&&output_.contains("Startup script returned successfully")){
            configured_=true;configurationOutput_.clear();
            queue_={QString("mavlink start -u 14561 -o 14560 -t %1 -m custom -r 50000").arg(gateway_),
                "mavlink stream -u 14561 -s HEARTBEAT -r 1",
                "mavlink stream -u 14561 -s ATTITUDE -r 20",
                "mavlink stream -u 14561 -s GLOBAL_POSITION_INT -r 10",
                "mavlink stream -u 14561 -s SYS_STATUS -r 1",
                "mavlink stream -u 14561 -s EXTENDED_SYS_STATE -r 2",
                "mavlink stream -u 14561 -s GPS_RAW_INT -r 5"};
            timeout_.stop();commands_.start();
        }
    });
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        reportFailure("WSL/PX4 süreç hatası: "+process_.errorString());if(error==QProcess::FailedToStart){owned_=false;timeout_.stop();emit ended();}
    });
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        timeout_.stop();commands_.stop();owned_=false;
        if(code!=0&&failure_.isEmpty())reportFailure(QString("PX4/WSL kod %1 ile kapandı. Ayrıntı için panel kayıtlarını inceleyin.").arg(code));
        emit log(QString("PX4 oturumu kapandı (%1).").arg(code));emit ended();
    });
}
SimulatorSession::~SimulatorSession(){
    discovery_.kill();discovery_.waitForFinished(500);
    if(running()){process_.write("__AERO_STOP__\n");process_.waitForFinished(7000);if(running()){process_.kill();process_.waitForFinished(500);}}
}
void SimulatorSession::query(const QStringList &args,Stage stage){
    stage_=stage;QStringList all={"-d",distribution_,"--exec"};all.append(args);
    emit log("WSL sorgusu: "+args.join(' '));discovery_.start(wslProgram(),all);queryTimeout_.start();
}
void SimulatorSession::start(){
    if(active())return;failure_.clear();
    if(wslProgram().isEmpty()){reportFailure("Windows wsl.exe bulunamadı.");emit ended();return;}
#ifdef Q_OS_WIN
    emit log(distribution_+" / PX4 kontrol ediliyor…");query({"pgrep","-x","px4"},Stage::Check);
#else
    reportFailure("Otomatik başlatıcı Windows + WSL2 içindir.");emit ended();
#endif
}
void SimulatorSession::launch(){
    QFile script(fleet_.isEmpty()?":/launcher/simulator_launcher.py":":/launcher/fleet_launcher.py");
    if(!script.open(QIODevice::ReadOnly)){reportFailure("Gömülü başlatıcı kaynağı bulunamadı.");emit ended();return;}
    configured_=false;output_.clear();profileHash_.clear();firmware_.clear();partition_.clear();profiles_.clear();owned_=true;
    emit log("PX4 klasörü: "+repository_);
    QFile profileScript(":/launcher/lab_profile.py");
    if(!profileScript.open(QIODevice::ReadOnly)){reportFailure("Model profili kaynağı bulunamadı.");owned_=false;emit ended();return;}
    QString source=QString::fromUtf8(profileScript.readAll())+"\n";
    if(!fleet_.isEmpty()){QFile f(":/launcher/fleet_profile.py");if(!f.open(QIODevice::ReadOnly)){reportFailure("Fleet profile kaynağı yok.");owned_=false;emit ended();return;}source+=QString::fromUtf8(f.readAll())+"\n";}
    source+=QString::fromUtf8(script.readAll());
    const QJsonObject config=fleet_.isEmpty()?profile_:QJsonObject{{"vehicles",fleet_},{"gateway",gateway_}};
    process_.start(wslProgram(),{"-d",distribution_,"--exec","python3","-u","-c",source,repository_,QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact))});timeout_.start();
}
void SimulatorSession::stop(){
    stage_=Stage::Idle;queryTimeout_.stop();discovery_.kill();commands_.stop();queue_.clear();
    if(running())process_.write("__AERO_STOP__\n");
    else {owned_=false;emit ended();}
}
