#pragma once
#include "LabTypes.hpp"
#include "LabTransport.hpp"
#include "SimulatorSession.hpp"
#include <QObject>
class ExperimentRunner final:public QObject {
    Q_OBJECT
public:
    ExperimentRunner(std::shared_ptr<FlightState>,SimulatorSession*,LabTransport*,QObject*);
    bool start(const QVector<LabSpec>&,QString& error);
    void tick(double now);
    void abort(const QString& reason="Operatör iptal etti.");
    bool busy()const{return phase_!=Phase::Idle;}
    bool measuring()const{return phase_==Phase::Measure;}
    const LabResult& current()const{return result_;}
    QString phaseText()const;
    int queueSize()const{return specs_.size();}
signals:
    void log(const QString&);
    void changed();
    void finished(const LabResult&);
    void batchFinished();
private:
    enum class Phase {Idle,Prepare,Arm,Takeoff,Settle,Measure,Cleanup,Land,Disarm,Restore,Between};
    Phase phase_=Phase::Idle;
    std::shared_ptr<FlightState> state_;SimulatorSession *session_;LabTransport* transport_;
    QVector<LabSpec> specs_;int runIndex_=0,point_=0,event_=0;
    double phaseAt_=0,startAt_=0,measureAt_=0,lastPosition_=0,lastPower_=0,lastActuators_=0,withinAt_=0,dwellAt_=0,landAt_=0;
    double lastTick_=0,freshSeconds_=0,climbAt_=0;bool actuatorRequested_=false,climbReached_=false,routeSent_=false,activeEvent_=false,eventEnding_=false;
    bool stopping_=false,complete_=false,unsafe_=false,sensorObserved_=false,modeObserved_=false;
    int initialMode_=0,lastSensorUnit_=0;bool sensorNeedsReset_=false;
    QString waiting_,fatal_;LabAction pendingAction_;
    QMap<QString,double> originals_,desired_;QStringList keys_;int parameter_=0;
    LabResult result_;aero::Stats horizontal_,vertical_,tilt_;aero::Energy energy_;
    void begin(double now);
    bool action(const LabAction&,const QString& context);
    void done(bool,const QString&,double);
    void setPhase(Phase,double);
    void cleanup(double);
    void finalise(double);
    void note(const QString&,const QString&,bool);
    void prepare(double);
    void land(double);
};
