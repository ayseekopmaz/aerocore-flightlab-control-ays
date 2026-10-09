#pragma once
#include "FlightState.hpp"
#include "LabTypes.hpp"
#include <QWidget>
#include <QJsonArray>
#include <QTimer>
class QSlider;class QLabel;class QPushButton;class QComboBox;class QPlainTextEdit;class QSpinBox;class MapView;class DroneView;class Scene3DView;class CameraPanel;
class SessionRecording {
public:
    ~SessionRecording(){QString error;save(error);}
    bool start(const QString&path,const QJsonObject&metadata);
    void updateMetadata(const QJsonObject&o){for(auto i=o.begin();i!=o.end();++i)metadata_[i.key()]=i.value();}
    bool capture(const FlightState&,double now);
    void event(const QString&,double now);
    bool save(QString&error);
    bool recording()const{return recording_;}
    QString path()const{return path_;}
    static std::optional<QJsonObject> load(const QString&,QString&);
    static bool applyFrame(const QJsonObject&,FlightState&,double now);
private:
    QString path_;QJsonObject metadata_;QJsonArray frames_,events_;bool recording_=false;double start_=0,last_=0;int eventBytes_=0;
};
class ReplayPanel final:public QWidget {
    Q_OBJECT
public:
    explicit ReplayPanel(QWidget*parent=nullptr);
    void openResult(const LabResult&);
private:
    std::shared_ptr<FlightState>state_;QJsonArray frames_,events_;QTimer timer_;
    QSlider*slider_;QLabel*status_;QPushButton*play_;QComboBox*speed_;QPlainTextEdit*eventsView_;MapView*map_;DroneView*drone_;Scene3DView*scene_;CameraPanel*camera_;QSpinBox*systemSpin_;
    double cursor_=0,duration_=0,lastTick_=0;QString title_;
    void open();void seek(double);void showFrame();
};
