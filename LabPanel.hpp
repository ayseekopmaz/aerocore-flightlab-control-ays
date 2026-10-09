#pragma once
#include "ExperimentRunner.hpp"
#include <QWidget>
#include <memory>
namespace Ui {class LabPanel;}
class LabChart;
class LabPanel final:public QWidget {
    Q_OBJECT
public:
    LabPanel(std::shared_ptr<FlightState>,SimulatorSession*,LabTransport*,QWidget*);
    ~LabPanel()override;
    LabSpec spec(QString& error)const;
    bool busy()const{return runner_->busy()||comparing_;}
    void compareFleet();
    void selectedVehicleChanged();
    void tick(double now);
    void addMapPoint(double lat,double lon);
    void sessionEnded();
    std::function<bool()> externalCommandPending;
signals:
    void log(const QString&);
    void routeChanged(const QVector<QPointF>&);
    void replayResult(const LabResult&);
private:
    std::unique_ptr<Ui::LabPanel> ui_;
    std::shared_ptr<FlightState> state_;SimulatorSession*session_;LabTransport*transport_;ExperimentRunner*runner_;
    LabChart* chart_;
    QVector<LabResult> results_;std::optional<LabResult> baseline_;
    LabSpec runSpec_;double regressionLimit_=20;
    bool comparing_=false,comparisonCancelled_=false;int comparisonSystem_=0;QVector<int> comparisonQueue_;LabSpec comparisonSpec_;QStringList comparisonIds_;
    void nextComparison();
    void applyVehicle(LabSpec&)const;
    void fill(const LabSpec&);
    void start(bool batch);
    void result(LabResult);
    void refreshResults();
    void selection();
    void save(bool csv);
    void publishRoute();
    void message(const QString&);
};
