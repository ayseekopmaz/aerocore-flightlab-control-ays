#pragma once
#include "FlightState.hpp"
#include <QWidget>
#include <QNetworkAccessManager>
#include <QCache>
#include <QSet>
#include <QPixmap>
#include <memory>
#include <functional>
class QLabel;
class MapView final:public QWidget {
public:
    explicit MapView(std::shared_ptr<FlightState> state,QWidget *parent=nullptr);
    void refresh();
    void setOnline(bool enabled);
    void followDrone();
    void setPlan(const QVector<QPointF>&p){plan_=p;update();}
    std::function<void(double,double)> pointAdded;
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
private:
    std::shared_ptr<FlightState> state_;
    QNetworkAccessManager network_;
    QCache<QString,QPixmap> tiles_{128};
    QSet<QString> pending_;
    QMap<QString,qint64> failures_;
    QLabel *credit_;
    QString cache_,notice_;
    QVector<QPointF> plan_;
    QPointF center_,drag_;
    int zoom_=17;
    bool initialized_=false,online_=false,follow_=true;
    QPointF world(double lat,double lon) const;
    void requestTile(int x,int y,const QString &key);
};
class DroneView final:public QWidget {
public:
    explicit DroneView(std::shared_ptr<FlightState> state,QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    std::shared_ptr<FlightState> state_;
};
