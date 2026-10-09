#include "LabTransport.hpp"
#include "SimulationProtocol.hpp"
#include "SimulatorSession.hpp"
#include <QUdpSocket>
#include <QFile>
#include <QTimer>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <cmath>
LabTransport::LabTransport(std::shared_ptr<FlightState>s,SimulatorSession*session,QUdpSocket*socket,QObject*p):QObject(p),state_(std::move(s)),session_(session),socket_(socket){
#ifdef Q_OS_WIN
    wind_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args){args->flags|=0x08000000;});
#endif
    wind_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&wind_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){if(!pending_||action_.type!="wind")return;auto bytes=wind_.readAll();auto o=QJsonDocument::fromJson(bytes).object();finish(code==0&&exit==QProcess::NormalExit&&o["ok"].toBool(),o["ok"].toBool()?o["evidence"].toString():o["error"].toString()+QString::fromUtf8(bytes));});
    connect(&wind_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){if(pending_&&action_.type=="wind")finish(false,wind_.errorString());});
}
void LabTransport::finish(bool ok,const QString&e,double v){pending_=false;emit completed(ok,e,v);}
bool LabTransport::write(const std::vector<std::uint8_t>&b,int system){return socket_->writeDatagram(reinterpret_cast<const char*>(b.data()),b.size(),QHostAddress(state_->peer),session_->remotePort(system))==qint64(b.size());}
bool LabTransport::send(const std::vector<std::uint8_t>&b){if(faultSystem_==state_->selected&&loss_>0&&std::uniform_real_distribution<double>(0,100)(rng_)<loss_)return true;if(faultSystem_==state_->selected&&delay_>0){if(outbound_.size()>=256)return false;outbound_.push_back({sitl::monoSeconds()+delay_/1000,state_->selected,b});return true;}return write(b,state_->selected);}
QVector<QNetworkDatagram> LabTransport::receive(QNetworkDatagram d,double now){if(d.senderPort()!=session_->remotePort(faultSystem_)||!faultSystem_)return {d};if(loss_>0&&std::uniform_real_distribution<double>(0,100)(rng_)<loss_)return {};if(delay_>0){if(inbound_.size()<512)inbound_.emplace_back(now+delay_/1000,std::move(d));return {};}return {d};}
QVector<QNetworkDatagram> LabTransport::drain(double now){QVector<QNetworkDatagram> out;while(!inbound_.empty()&&inbound_.front().first<=now){out.append(inbound_.front().second);inbound_.pop_front();}return out;}
void LabTransport::resetFault(){loss_=delay_=0;faultSystem_=0;inbound_.clear();outbound_.clear();}
void LabTransport::cancel(){++epoch_;resetFault();pending_=false;wind_.kill();}
bool LabTransport::issue(const LabAction &a){
    if(pending_||!session_->owned()||!session_->running()||state_->peer.isEmpty())return false;
    const auto epoch=++epoch_;action_=a;pending_=true;deadline_=sitl::monoSeconds()+10;auto sys=std::uint8_t(state_->selected);bool sent=false;
    if(a.type=="link"){faultSystem_=state_->selected;loss_=a.a;delay_=a.b;inbound_.clear();outbound_.clear();QTimer::singleShot(0,this,[this,epoch]{if(!pending_||epoch_!=epoch)return;finish(true,"Panel↔PX4 MAVLink hattında çift yönlü kayıp/gecikme uygulandı; RNG=20261008");});return true;}
    if(a.type=="wind"){
        if(wind_.state()!=QProcess::NotRunning){pending_=false;return false;}
        QFile file(":/launcher/wind_action.py");if(!file.open(QIODevice::ReadOnly)){pending_=false;return false;}
        auto wsl=QStandardPaths::findExecutable("wsl.exe");
#ifdef Q_OS_WIN
        if(wsl.isEmpty())wsl=QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot","C:/Windows")+"/System32/wsl.exe");
#endif
        wind_.start(wsl,{"-d",session_->distribution(),"--exec","python3","-u","-c",QString::fromUtf8(file.readAll()),QString::number(a.a,'g',15),QString::number(a.b,'g',15),session_->partition()});return true;
    }
    if(a.type=="read")sent=send(sitl::paramRead(sys,a.name.toStdString(),sequence_++));
    if(a.type=="set"){auto &cache=parameters_[state_->selected];auto it=cache.constFind(a.name);if(it==cache.cend()||!(it->type==6||it->type==9)||(it->type==6&&std::floor(a.a)!=a.a)){pending_=false;return false;}sent=send(sitl::paramSet(sys,a.name.toStdString(),float(a.a),it->type,sequence_++));}
    if(a.type=="command")sent=send(sitl::commandLong(std::uint16_t(a.command),sys,1,a.params,sequence_++));
    if(a.type=="goto")sent=send(sitl::reposition(sys,a.a,a.b,float(a.c),float(a.d),sequence_++));
    if(!sent)pending_=false;return sent;
}
void LabTransport::feed(const sitl::TelemetryPacketData&p){if(p.system!=state_->selected||p.component!=1)return;
    if(p.kind==sitl::PacketKind::Parameter){QString name=QString::fromStdString(p.paramName);if((p.paramType==6||p.paramType==9)&&std::isfinite(p.paramValue))parameters_[p.system][name]={p.paramValue,p.paramType};if(pending_&&name==action_.name&&(action_.type=="read"||action_.type=="set")){
        bool valid=(p.paramType==6||p.paramType==9)&&std::isfinite(p.paramValue);if(action_.type=="set")valid&=std::abs(p.paramValue-action_.a)<=std::max(1e-5,std::abs(action_.a)*1e-5);finish(valid,valid?"PARAM_VALUE readback doğrulandı":"Parametre tipi / değeri uyuşmadı",p.paramValue);}}
    if(pending_&&p.kind==sitl::PacketKind::CommandAck&&(action_.type=="command"||action_.type=="goto")&&p.command==(action_.type=="goto"?192:action_.command)&&(p.ackTargetSystem==0||p.ackTargetSystem==255)&&(p.ackTargetComponent==0||p.ackTargetComponent==190)){
        if(p.result==5){deadline_=sitl::monoSeconds()+10;return;}finish(p.result==0,QString("COMMAND_ACK %1 / result %2").arg(p.command).arg(p.result));}
}
void LabTransport::tick(double now){while(!outbound_.empty()&&outbound_.front().at<=now){write(outbound_.front().bytes,outbound_.front().system);outbound_.pop_front();}if(pending_&&now>deadline_){wind_.kill();finish(false,"Eylem onayı 10 sn içinde alınamadı; sonuç bilinmiyor.");}}

bool LabTransport::heartbeat(const std::vector<std::uint8_t>&b){
    const int selected=state_->selected;bool ok=true;
    for(int n=1;n<=session_->vehicleCount();++n){
        if(n==selected)ok=send(b)&&ok;
        else if(n==faultSystem_){if(loss_>0&&std::uniform_real_distribution<double>(0,100)(rng_)<loss_)continue;if(delay_>0){if(outbound_.size()>=256)ok=false;else outbound_.push_back({sitl::monoSeconds()+delay_/1000,n,b});}else ok=write(b,n)&&ok;}
        else ok=write(b,n)&&ok;
    }return ok;
}
