#pragma once
#include "FlightState.hpp"
#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <deque>
#include <random>
class QUdpSocket;class SimulatorSession;
struct LabAction {QString type,name;double a=0,b=0,c=0,d=0;int command=0;std::array<float,7> params{};};
class LabTransport:public QObject {
    Q_OBJECT
public:
    LabTransport(std::shared_ptr<FlightState>,SimulatorSession*,QUdpSocket*,QObject*);
    virtual bool issue(const LabAction&);
    void feed(const sitl::TelemetryPacketData&);
    void tick(double now);
    bool send(const std::vector<std::uint8_t>&);
    bool heartbeat(const std::vector<std::uint8_t>&);
    QVector<QNetworkDatagram> receive(QNetworkDatagram,double now);
    QVector<QNetworkDatagram> drain(double now);
    virtual void resetFault();
    bool busy()const{return pending_;}
    virtual void cancel();
signals:
    void completed(bool ok,const QString& evidence,double value);
private:
    std::shared_ptr<FlightState> state_;SimulatorSession *session_;QUdpSocket* socket_;QProcess wind_;
    quint64 epoch_=0;
    LabAction action_;bool pending_=false;double deadline_=0,loss_=0,delay_=0;std::uint8_t sequence_=0;int faultSystem_=0;
    struct Parameter{float value;std::uint8_t type;};QMap<int,QMap<QString,Parameter>> parameters_;
    std::mt19937 rng_{20261008};std::deque<std::pair<double,QNetworkDatagram>> inbound_;
    struct Outbound{double at;int system;std::vector<std::uint8_t> bytes;};
    std::deque<Outbound> outbound_;
    void finish(bool,const QString&,double=0);
    bool write(const std::vector<std::uint8_t>&,int system);
};
