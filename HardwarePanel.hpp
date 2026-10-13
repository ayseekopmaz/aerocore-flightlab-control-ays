#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QMap>
#include <QJsonArray>
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QFormLayout;
class QPushButton;
class QComboBox;
class HardwarePanel final : public QWidget {
    Q_OBJECT
public:
    explicit HardwarePanel(QWidget *parent=nullptr);
    bool enabledForSimulation() const;
    bool relaySelected()const;
    bool windSelected()const;
    void setRelayTelemetry(const QJsonArray&);
    QJsonObject simulationProfile(QString &error) const;
    void setMissionStatus(const QString&);
    void setMissionBusy(bool);
signals:
    void startIhaRequested();
    void stopIhaRequested();
    void exportResultsPdfRequested();
private:
    QPushButton *startMission_=nullptr,*stopMission_=nullptr;
    QLabel *missionStatus_=nullptr;
    QComboBox *missionMode_=nullptr;
    QWidget *relayView_=nullptr;
    QMap<QString,QDoubleSpinBox*> fields_;
    QCheckBox *use_=nullptr,*camera_=nullptr;
    QLabel *status_=nullptr;
    QJsonObject reference_;
    void field(QFormLayout*,const QString&,const QString&,double,double,double,int=2);
    bool fill(const QJsonObject&,QString&);
    void assess();
};
