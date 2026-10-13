#pragma once
#include "LabTransport.hpp"
#include "LabTypes.hpp"
#include "SimulatorSession.hpp"
#include "RelayCore.hpp"
#include "FormationCore.hpp"
#include <QObject>
#include "TelemetryGrace.hpp"
#include <array>
#include <memory>
class QUdpSocket;
class WindFormationCoordinator final:public QObject{
    Q_OBJECT
public:
    WindFormationCoordinator(std::shared_ptr<FlightState>,SimulatorSession*,QUdpSocket*,QObject*);
    bool request(QString&);bool busy()const{return pending_||running_;}
    void tick(double);void feed(const sitl::TelemetryPacketData&);void abort();void sessionEnded();
signals:
    void log(const QString&);void status(const QString&);void telemetry(const QJsonArray&);void finished(const LabResult&);
private:
    enum class Phase{Prepare,Formation,Measure,Lower,Swap,RestoreLower,Restore,Return,Landing};
    struct Agent{aero::TelemetryGrace grace;std::shared_ptr<FlightState>state;LabTransport*transport=nullptr;QString context;int stage=0;aura::Battery battery;LabResult result;double commandAt=0;};
    std::array<Agent,3>agents_;std::shared_ptr<FlightState>source_;SimulatorSession*session_;
    Phase phase_=Phase::Prepare;bool pending_=false,running_=false,failed_=false,windOn_=false,windOff_=false,stopping_=false,adapted_=false,restored_=false,baselineValid_=false;
    int trial_=0,leader_=0;double requestAt_=0,startAt_=0,phaseAt_=0,measureAt_=0,lastTick_=0,lastUi_=0,lastTargets_=0,center_=10,formationError_=0,coverageS_=0,stressSince_=0;
    double originLat_=0,originLon_=0;std::array<double,3>homeEast_{},homeNorth_{};std::array<double,2>totals_{};double baseCurrentSum_=0,baseCurrentTime_=0;QJsonArray events_;QString reason_;
    bool live(int,double)const;bool ground(double)const;bool arrived(const std::array<shield::Target,3>&,double)const;
    void sync();void launch(double);void phase(Phase,double);void targets(const std::array<shield::Target,3>&,double);
    bool issue(int,const LabAction&,const QString&);void complete(int,bool,const QString&);void wind(bool);void stop(const QString&);void finalise(double);
};
