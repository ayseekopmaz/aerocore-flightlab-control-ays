#pragma once
#include "LabTypes.hpp"
#include "FlightState.hpp"
#include <memory>
#include <QWidget>
#include <QJsonArray>
class QSpinBox; class QTableWidget; class QLabel;
struct VehicleProfile {
    int system=1; QString name="Drone 1",batteryModel="PX4 synthetic";
    QJsonObject physical{{"payload",0},{"cgX",0},{"cgY",0},{"cgZ",0},{"gpsNoise",0}};
    QMap<QString,double> parameters;
    QJsonObject json()const;
    static std::optional<VehicleProfile> fromJson(const QJsonObject&,QString&);
    void apply(LabSpec&)const;
};
class FleetPanel final:public QWidget {
    Q_OBJECT
public:
    explicit FleetPanel(QWidget*parent=nullptr);
    QJsonArray profiles(QString&error)const;
    void setActive(bool);
    int count()const;
    void bindState(std::shared_ptr<FlightState>);
signals:
    void compareRequested();
private:
    QSpinBox*count_;QTableWidget*table_;QLabel*notice_;
    void save();void load();
};
