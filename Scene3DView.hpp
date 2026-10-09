#pragma once
#include "FlightState.hpp"
#include <QWidget>
#include <QVector3D>
#include <QMap>
#include <functional>
#include <memory>

// Perspective scene using live/replayed MAVLink coordinates. The ground grid is
// an operator reference plane; it is not a Gazebo framebuffer or terrain mesh.
class Scene3DView final:public QWidget {
    Q_OBJECT
public:
    explicit Scene3DView(std::shared_ptr<FlightState>,QWidget*parent=nullptr);
    void setPlan(const QVector<QPointF>&route){plan_=route;update();}
    void followSelected(){follow_=true;update();}
    void setFollowing(bool on){follow_=on;update();}
    void resetView();
    void resetOrigin(){originReady_=false;center_={};follow_=true;update();}
    std::function<bool(int)> selected;
protected:
    void paintEvent(QPaintEvent*)override;
    void wheelEvent(QWheelEvent*)override;
    void mousePressEvent(QMouseEvent*)override;
    void mouseMoveEvent(QMouseEvent*)override;
    void mouseReleaseEvent(QMouseEvent*)override;
private:
    std::shared_ptr<FlightState>state_;
    QVector<QPointF>plan_;QMap<int,QPointF>hit_;
    QPointF lastMouse_,center_;double originLat_=0,originLon_=0,azimuth_=38,elevation_=36,distance_=65;
    bool originReady_=false,dragged_=false,follow_=true;
    void ensureOrigin(double now);
    QVector3D toWorld(double lat,double lon,double altitude)const;
};
