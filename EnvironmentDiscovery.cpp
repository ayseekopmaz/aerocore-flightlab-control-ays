#include "EnvironmentDiscovery.hpp"
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
QString wslProgram(){
#ifdef Q_OS_WIN
    const auto system=qEnvironmentVariable("SystemRoot","C:/Windows");
    for(const auto &path:{system+"/System32/wsl.exe",system+"/Sysnative/wsl.exe"})
        if(QFile::exists(path))return path;
#endif
    return QStandardPaths::findExecutable("wsl.exe");
}
QString decode(QByteArray bytes){
    if(bytes.startsWith("\xFF\xFE"))bytes.remove(0,2);
    if(bytes.contains('\0')){
        QString result;
        for(qsizetype i=0;i+1<bytes.size();i+=2)
            result.append(QChar(uchar(bytes[i])|(ushort(uchar(bytes[i+1]))<<8)));
        return result;
    }
    return QString::fromUtf8(bytes);
}
}
QString EnvironmentDiscovery::chooseDistribution(const QByteArray &raw){
    for(auto line:decode(raw).split('\n')){
        line=line.trimmed();line.remove(QChar::ByteOrderMark);
        if(!line.isEmpty()&&!line.startsWith("docker-desktop",Qt::CaseInsensitive))return line;
    }
    return {};
}
EnvironmentDiscovery::EnvironmentDiscovery(QObject *parent):QObject(parent){
#ifdef Q_OS_WIN
    process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args){args->flags|=CREATE_NO_WINDOW;});
#endif
    process_.setProcessChannelMode(QProcess::MergedChannels);
    timeout_.setSingleShot(true);
    connect(&timeout_,&QTimer::timeout,this,[this]{
        process_.kill();finish("WSL/PX4 taraması zaman aşımına uğradı. Alanları elle doldurabilirsiniz.");
    });
    connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart&&busy())finish("WSL çalıştırılamadı: "+process_.errorString());
    });
    connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
        if(!busy())return;
        const auto output=process_.readAll();
        if(stage_==Stage::List){
            if(code!=0||exit!=QProcess::NormalExit){finish("WSL dağıtım listesi alınamadı: "+decode(output).trimmed());return;}
            distribution_=chooseDistribution(output);
            if(distribution_.isEmpty()){finish("Kurulu Linux dağıtımı bulunamadı. WSL2 ve Ubuntu kurulumu gerekli.");return;}
            QFile script(":/launcher/discover_px4.py");
            if(!script.open(QIODevice::ReadOnly)){finish("PX4 tarama betiği kaynaklarda bulunamadı.");return;}
            stage_=Stage::Locate;timeout_.start(30000);
            process_.start(wslProgram(),{"-d",distribution_,"--exec","python3","-c",QString::fromUtf8(script.readAll())});
            return;
        }
        if(code!=0||exit!=QProcess::NormalExit){finish("PX4 klasörü aranamadı: "+decode(output).trimmed());return;}
        const auto path=decode(output).trimmed();
        if(!path.startsWith('/')||path.contains('\n')||path.contains('\r')){
            finish("PX4 klasörü bulunamadı. WSL içindeki kaynak yolunu elle girin.");return;
        }
        emit found(distribution_,path);
        finish();
    });
}
EnvironmentDiscovery::~EnvironmentDiscovery(){
    timeout_.stop();
    if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(500);}
}
void EnvironmentDiscovery::finish(const QString &error){
    timeout_.stop();stage_=Stage::Idle;
    emit busyChanged(false);
    if(!error.isEmpty())emit message(error);
}
void EnvironmentDiscovery::start(){
    if(busy())return;
    const auto wsl=wslProgram();
    if(wsl.isEmpty()){emit message("Windows wsl.exe bulunamadı. WSL2 kurulumu gerekli; alanlar elle düzenlenebilir.");return;}
    stage_=Stage::List;emit busyChanged(true);timeout_.start(15000);
    process_.start(wsl,{"--list","--quiet"});
}
