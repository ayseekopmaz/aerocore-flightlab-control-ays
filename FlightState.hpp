#pragma once
#include "MavlinkReadOnly.hpp"
#include <QMap>
#include <QPointF>
#include <QVector>
struct FlightVehicle {
    double last=0,posAt=0,attitudeAt=0,batteryAt=0,heartbeatAt=0,landedAt=0;
    double lat=0,lon=0,alt=0,amsl=0,roll=0,pitch=0,yaw=0;
    std::array<float,32> actuators{};quint32 activeOutputs=0;double actuatorsAt=0;
    double voltage=0,current=0,powerAt=0,gpsAt=0;bool powerValid=false;quint32 customMode=0,bootMs=0;int fixType=0;
    int battery=-1; bool armed=false,px4=false; int landed=0;
    quint64 packets=0;
    QVector<QPointF> trail; // longitude, latitude; bounded to 600 samples.
};
struct FlightState {
    QMap<int,FlightVehicle> vehicles;
    sitl::MavlinkParser parser;
    int selected=1;
    QString peer;
};
