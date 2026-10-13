#pragma once
#include "ExperimentRunner.hpp"
#include <QObject>
#include <array>
class QUdpSocket;
class IhaMissionCoordinator final:public QObject {
    Q_OBJECT
public:
    IhaMissionCoordinator(std::shared_ptr<FlightState>,SimulatorSession*,QUdpSocket*,QObject*);
    bool request(QString&);
    bool busy()const{return pending_||running_;}
    void tick(double);
    void feed(const sitl::TelemetryPacketData&);
    void abort();
    void sessionEnded();
signals:
    void log(const QString&);
    void status(const QString&);
    void finished(const LabResult&);
private:
    struct Agent{std::shared_ptr<FlightState> state;LabTransport*transport=nullptr;ExperimentRunner*runner=nullptr;};
    std::array<Agent,3> agents_;
    std::shared_ptr<FlightState> source_;SimulatorSession*session_;
    bool pending_=false,running_=false,cancelling_=false,barrierOpen_=false;
    double requestedAt_=0,barrierAt_=0;
    void sync();
    bool ready(double)const;
    bool launch(QString&);
};
