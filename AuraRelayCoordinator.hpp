#pragma once
#include "LabTransport.hpp"
#include "LabTypes.hpp"
#include "SimulatorSession.hpp"
#include "RelayCore.hpp"
#include <QObject>
#include "TelemetryGrace.hpp"
#include <array>
#include <memory>
class QUdpSocket;
class AuraRelayCoordinator final:public QObject {
    Q_OBJECT
public:
    AuraRelayCoordinator(std::shared_ptr<FlightState>,SimulatorSession*,QUdpSocket*,QObject*);
    bool request(QString&);
    bool busy()const{return pending_||running_;}
    void tick(double);
    void feed(const sitl::TelemetryPacketData&);
    void abort();
    void sessionEnded();
signals:
    void log(const QString&);
    void status(const QString&);
    void telemetry(const QJsonArray&);
    void finished(const LabResult&);
private:
    enum class Role {Standby,Arm,Takeoff,Transit,Patrol,Return,Landing,Disarm,Charging,Fault};
    struct Agent {aero::TelemetryGrace grace;std::shared_ptr<FlightState>state;LabTransport*transport=nullptr;Role role=Role::Standby;aura::Battery battery;aura::HandoverGate gate;QString command;double roleAt=0;bool routeSent=false,coveredEnergy=true;int point=0;double dwellAt=0;LabResult result;};
    std::array<Agent,3> agents_;std::shared_ptr<FlightState>source_;SimulatorSession*session_;
    bool pending_=false,running_=false,stopping_=false,failed_=false,windSent_=false,windRestored_=false,coverageStarted_=false;
    double requestAt_=0,startAt_=0,lastTick_=0,lastUi_=0,coverageAt_=0,stopAt_=0;int active_=0,replacement_=-1,handovers_=0;
    double originLat_=0,originLon_=0;QVector<aero::Point>route_;aura::Coverage coverage_;QJsonArray events_;QString failure_;
    double duration_=900,trigger_=.25,critical_=.1,altitude_=8,radius_=25,chargeA_=1.3,windAt_=30,windSpeed_=3;
    bool live(int,double)const;bool inZone(int,double)const;bool allGround(double)const;
    void sync();void setRole(int,Role,double);void event(const QString&,int,const QString&);
    bool command(int,const LabAction&,const QString&);void complete(int,bool,const QString&);
    void launch(double);void step(int,double);void stop(const QString&);void finalise();
};
