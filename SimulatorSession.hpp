#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
class SimulatorSession:public QObject {
    Q_OBJECT
public:
    explicit SimulatorSession(QObject *parent=nullptr);
    ~SimulatorSession() override;
    void start();
    void setConfiguration(const QString &distribution,const QString &repository){distribution_=distribution;repository_=repository;}
    void setLabProfile(const QJsonObject &p){profile_=p;}
    virtual QJsonObject labProfile()const{return profiles_.contains(activeSystem_)?profiles_[activeSystem_]["profile"].toObject():profile_;}
    void setFleet(const QJsonArray&v){fleet_=v;}
    void setActiveSystem(int n){activeSystem_=n;}
    QJsonObject vehicleProfile(int n)const{if(profiles_.contains(n))return profiles_.value(n);for(auto v:fleet_)if(v.toObject()["system"].toInt()==n)return v.toObject();return {};}
    int vehicleCount()const{return fleet_.isEmpty()?1:fleet_.size();}
    QJsonArray fleetProfiles()const{return fleet_;}
    quint16 remotePort(int n)const{return quint16(14561+n-1);}
    QString partition()const{return partition_;}
    virtual QString firmware()const{return firmware_;}
    virtual QString profileHash()const{return profiles_.contains(activeSystem_)?profiles_[activeSystem_]["sha256"].toString():profileHash_;}
    QString distribution()const{return distribution_;}
    QString repository()const{return repository_;}
    void stop(); // Graceful PX4 console shutdown; never terminates other WSL sessions.
    virtual bool running() const {return process_.state()!=QProcess::NotRunning;}
    bool active() const {return running()||discovery_.state()!=QProcess::NotRunning;}
    virtual bool owned() const {return owned_;}
signals:
    void log(const QString &line);
    void failure(const QString &message);
    void networkReady(const QString &wslIp,const QString &windowsIp);
    void ready();
    void ended();
private:
    enum class Stage{Idle,Check,Host,Route};
    Stage stage_=Stage::Idle;
    QProcess discovery_,process_;
    QTimer timeout_,commands_,queryTimeout_;
    QString host_,gateway_,output_,configurationOutput_;
    QString distribution_="Ubuntu-24.04",repository_="~/PX4-Autopilot";
    QString failure_,profileHash_,firmware_,partition_;
    QJsonObject profile_;
    QJsonArray fleet_;QMap<int,QJsonObject> profiles_;int activeSystem_=1;
    QStringList queue_;
    bool owned_=false,configured_=false;
    void query(const QStringList &args,Stage stage);
    void launch();
    void reportFailure(const QString &message);
};
