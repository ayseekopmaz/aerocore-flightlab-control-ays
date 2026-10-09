#pragma once
#include "ExperimentCore.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>
#include <QMap>
#include <QPointF>
#include <optional>
struct LabEvent {double at=10,duration=5,a=3,b=0;QString type="wind";};
struct LabSpec {
    QString name="Hover testi",kind="hover",vehicle="x500",versionTag="baseline";
    double altitude=5,duration=30,speed=3,dwell=2,maxError=2,maxAltError=1,maxTilt=30,maxTime=180,minCoverage=.8;
    double windSpeed=0,windDirection=0,payload=0,cgX=0,cgY=0,cgZ=0,gpsNoise=0;
    double batteryDrain=0,batteryMin=10;bool energyRequired=false;
    QString batteryModel="PX4 synthetic",expectedMode="";
    QMap<QString,double> parameters;
    QVector<aero::Point> route;
    QVector<LabEvent> events;
    QJsonObject json()const;
    QJsonObject profile()const;
    static std::optional<LabSpec> fromJson(const QJsonObject&,QString& error);
    QString validate()const;
};
struct LabSample {double t=0,boot=0,east=0,north=0,alt=0,error=0,altError=0,roll=0,pitch=0,yaw=0;int mode=0,battery=-1;bool measured=false;};
struct LabResult {
    QString id,status="INCONCLUSIVE",reason,started,firmware,profileHash;
    LabSpec spec;
    QJsonObject metrics;
    QJsonArray eventLog;
    QVector<LabSample> samples;
    double originLat=0,originLon=0;
    QJsonObject json()const;
    static std::optional<LabResult> fromJson(const QJsonObject&,QString&);
};
QString labKindName(const QString&);
LabSpec labPreset(const QString&);
