#include "FlightViews.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QFile>
#include <array>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace {constexpr double pi=3.141592653589793;}
MapView::MapView(std::shared_ptr<FlightState> state,QWidget *parent):QWidget(parent),state_(std::move(state)){
    setMinimumSize(360,300);setMouseTracking(true);
    cache_=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/osm";QDir().mkpath(cache_);
    credit_=new QLabel("<a href='https://www.openstreetmap.org/copyright'>© OpenStreetMap contributors</a>",this);
    credit_->setOpenExternalLinks(true);credit_->setStyleSheet("background:white;padding:5px;color:#17365D;font-size:11px;");credit_->adjustSize();
}
QPointF MapView::world(double lat,double lon) const{
    lat=std::clamp(lat,-85.05112878,85.05112878);const double size=256.0*std::pow(2,zoom_);
    return {(lon+180)/360*size,(1-std::asinh(std::tan(lat*pi/180))/pi)/2*size};
}
void MapView::refresh(){
    auto it=state_->vehicles.constFind(state_->selected);
    if(it!=state_->vehicles.cend()&&it->posAt>0&&sitl::monoSeconds()-it->posAt<3&&(!initialized_||follow_)){
        center_=world(it->lat,it->lon);initialized_=true;
    }
    update();
}
void MapView::followDrone(){follow_=true;refresh();}
void MapView::setOnline(bool enabled){online_=enabled;notice_.clear();credit_->setVisible(enabled);update();}
void MapView::resizeEvent(QResizeEvent *){credit_->move(width()-credit_->width()-8,height()-credit_->height()-8);}
void MapView::requestTile(int x,int y,const QString &key){
    if(pending_.contains(key)||pending_.size()>=4)return;
    const qint64 now=QDateTime::currentMSecsSinceEpoch();
    if(failures_.value(key)>now)return;
    const QString path=cache_+"/"+key+".png";
    // Seven-day minimum persistent cache; only visible tiles are requested.
    const QFileInfo info(path);
    if(info.exists()&&info.lastModified().msecsTo(QDateTime::currentDateTimeUtc())<7LL*86400000){
        auto *image=new QPixmap(path);if(!image->isNull()){tiles_.insert(key,image);return;}delete image;
    }
    if(QDir(cache_).entryList({"*.png"},QDir::Files).size()>=1000){
        // Remove expired cache files only. Do not evict fresh tiles and redownload them.
        for(const auto &file:QDir(cache_).entryInfoList({"*.png"},QDir::Files))
            if(file.lastModified().msecsTo(QDateTime::currentDateTimeUtc())>=7LL*86400000)QFile::remove(file.absoluteFilePath());
        if(QDir(cache_).entryList({"*.png"},QDir::Files).size()>=1000){notice_="Harita önbelleği dolu";return;}
    }
    pending_.insert(key);
    QNetworkRequest request(QUrl(QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(zoom_).arg(x).arg(y)));
    request.setRawHeader("User-Agent","AeroCoreSimulator/0.4.5 (Qt desktop; local PX4 SITL)");
    request.setTransferTimeout(10000);
    auto *reply=network_.get(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply,key,path]{
        pending_.remove(key);
        if(reply->error()==QNetworkReply::NoError&&reply->bytesAvailable()<2*1024*1024){
            const auto bytes=reply->readAll();auto *image=new QPixmap;
            if(image->loadFromData(bytes,"PNG")){
                tiles_.insert(key,image);QSaveFile file(path);if(file.open(QIODevice::WriteOnly)){file.write(bytes);file.commit();}
                notice_.clear();
            }else{delete image;failures_[key]=QDateTime::currentMSecsSinceEpoch()+60000;notice_="Harita görseli alınamadı";}
        }else{failures_[key]=QDateTime::currentMSecsSinceEpoch()+60000;notice_="Harita bağlantısı yok — telemetri çalışmaya devam eder";}
        reply->deleteLater();update();
    });
}
void MapView::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#FFF8FC"));
    if(!initialized_){p.setPen(QColor("#17365D"));p.drawText(rect(),Qt::AlignCenter,"HARİTA / Konum telemetrisi bekleniyor");return;}
    const int count=1<<zoom_;const double worldSize=256.0*count;
    const QPointF origin=center_-QPointF(width()/2.,height()/2.);
    int x0=int(std::floor(origin.x()/256)),y0=int(std::floor(origin.y()/256));
    for(int x=x0;x<=int(std::floor((origin.x()+width())/256));++x)
        for(int y=y0;y<=int(std::floor((origin.y()+height())/256));++y){
            if(y<0||y>=count)continue;int wrapped=(x%count+count)%count;
            QString key=QString("%1_%2_%3").arg(zoom_).arg(wrapped).arg(y);
            QRectF box(x*256-origin.x(),y*256-origin.y(),256,256);
            if(online_&&!tiles_.contains(key))requestTile(wrapped,y,key);
            if(online_&&tiles_.contains(key))p.drawPixmap(box.toRect(),*tiles_.object(key));
            else {p.setPen(QColor("#CBD5E1"));p.drawRect(box);}
        }
    const auto screen=[&](double lat,double lon){auto q=world(lat,lon);double dx=q.x()-center_.x();dx-=std::round(dx/worldSize)*worldSize;return QPointF(width()/2.+dx,height()/2.+q.y()-center_.y());};
    if(!plan_.isEmpty()){
        QPainterPath route;for(int i=0;i<plan_.size();++i){auto q=world(plan_[i].y(),plan_[i].x())-origin;if(i==0)route.moveTo(q);else route.lineTo(q);p.setPen(QColor("#2563EB"));p.drawEllipse(q,5,5);p.drawText(q+QPointF(7,-7),QString::number(i));}
        p.setPen(QPen(QColor("#2563EB"),2,Qt::DashLine));p.drawPath(route);
    }
    for(auto it=state_->vehicles.cbegin();it!=state_->vehicles.cend();++it){
        if(it->posAt==0)continue;
        if(!it->trail.isEmpty()){
            QPainterPath path;path.moveTo(screen(it->trail.first().y(),it->trail.first().x()));
            for(const auto &q:it->trail)path.lineTo(screen(q.y(),q.x()));p.setPen(QPen(it.key()==1?QColor("#EC4899"):it.key()==2?QColor("#EF4444"):QColor("#FACC15"),it.key()==state_->selected?3:2));p.drawPath(path);
        }
        auto q=screen(it->lat,it->lon);const bool fresh=sitl::monoSeconds()-it->posAt<3;
        p.save();p.translate(q);p.rotate(it->yaw);p.setPen(QPen(Qt::white,2));
        p.setBrush(fresh?(it.key()==1?QColor("#EC4899"):it.key()==2?QColor("#EF4444"):QColor("#FACC15")):QColor("#94A3B8"));
        p.drawPolygon(QPolygonF(QList<QPointF>{QPointF(0,-15),QPointF(10,10),QPointF(0,5),QPointF(-10,10)}));p.restore();
        p.setPen(it.key()==3?QColor("#854D0E"):QColor("#17365D"));p.setBrush(Qt::white);p.drawRect(QRectF(q.x()+15,q.y()-12,95,22));
        p.drawText(QPointF(q.x()+20,q.y()+3),QString("SYS %1 %2").arg(it.key()).arg(fresh?"":"ESKİ"));
    }
    p.fillRect(QRect(0,0,width(),28),QColor(255,255,255,230));p.setPen(QColor("#17365D"));
    p.drawText(10,19,notice_.isEmpty()?QString("%1 • Yakınlık %2 • Tekerlek: zoom / sürükle: gez").arg(online_?"OSM harita":"Çevrimdışı koordinat görünümü").arg(zoom_):notice_);
}
void MapView::wheelEvent(QWheelEvent *e){
    if(!initialized_)return;int next=std::clamp(zoom_+(e->angleDelta().y()>0?1:-1),3,19);
    center_*=std::pow(2,next-zoom_);zoom_=next;update();e->accept();
}
void MapView::mousePressEvent(QMouseEvent *e){drag_=e->position();}
void MapView::mouseMoveEvent(QMouseEvent *e){if(initialized_&&(e->buttons()&Qt::LeftButton)){follow_=false;center_-=e->position()-drag_;drag_=e->position();const double size=256.*(1<<zoom_);center_.setX(std::fmod(center_.x()+size,size));center_.setY(std::clamp(center_.y(),0.,size));update();}}

DroneView::DroneView(std::shared_ptr<FlightState> state,QWidget *parent):QWidget(parent),state_(std::move(state)){setMinimumSize(320,300);}
void DroneView::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#FFF8FC"));
    auto it=state_->vehicles.constFind(state_->selected);
    p.setPen(QColor("#17365D"));p.drawText(14,24,"DRONE / Telemetri tabanlı 3B model");
    if(it==state_->vehicles.cend()||it->attitudeAt==0||sitl::monoSeconds()-it->attitudeAt>3){p.drawText(rect(),Qt::AlignCenter,"Canlı açı verisi bekleniyor");return;}
    const bool armed=it->armed&&sitl::monoSeconds()-it->heartbeatAt<3;
    QMatrix4x4 projection,view,model;
    projection.perspective(42,float(width())/std::max(1,height()),0.1f,100.f);
    view.lookAt(QVector3D(5,-7,5),QVector3D(0,0,0.6),QVector3D(0,0,1));
    // Body x=forward, y=right. NED telemetry is converted to z-up visualization.
    model.translate(0,0,float(it->posAt>0&&sitl::monoSeconds()-it->posAt<3?std::clamp(it->alt/8.,0.,2.):0));
    model.rotate(float(-it->yaw),0,0,1);model.rotate(float(-it->pitch),0,1,0);model.rotate(float(it->roll),1,0,0);
    const auto project=[&](QVector3D q,const QMatrix4x4 &m){auto v=projection*view*m*QVector4D(q,1);return QPointF((v.x()/v.w()+1)*width()/2.,(1-v.y()/v.w())*height()/2.);};
    QMatrix4x4 identity;p.setPen(QPen(QColor("#C8D9ED"),1));
    for(int n=-4;n<=4;++n){p.drawLine(project({float(n),-4,0},identity),project({float(n),4,0},identity));p.drawLine(project({-4,float(n),0},identity),project({4,float(n),0},identity));}
    const std::array<QVector3D,4> motors={QVector3D(1,1,0),QVector3D(1,-1,0),QVector3D(-1,-1,0),QVector3D(-1,1,0)};
    for(const auto &motor:motors){p.setPen(QPen(motor.x()>0?QColor("#2563EB"):QColor("#17365D"),9,Qt::SolidLine,Qt::RoundCap));p.drawLine(project({0,0,0},model),project(motor,model));}
    p.setPen(QPen(QColor("#17365D"),2));p.setBrush(QColor("#DCE6F2"));
    p.drawPolygon(QPolygonF(QList<QPointF>{project({.5f,.35f,0},model),project({.5f,-.35f,0},model),project({-.5f,-.35f,0},model),project({-.5f,.35f,0},model)}));
    const double phase=armed?sitl::monoSeconds()*18:0;
    for(const auto &motor:motors){
        QPainterPath ring;for(int n=0;n<=32;++n){double a=n*2*pi/32;auto q=project(motor+QVector3D(float(.38*std::cos(a)),float(.38*std::sin(a)),.08f),model);if(n==0)ring.moveTo(q);else ring.lineTo(q);}
        p.setPen(QPen(QColor("#64748B"),1));p.drawPath(ring);
        p.setPen(QPen(armed?QColor("#2563EB"):QColor("#17365D"),4));
        auto d=QVector3D(float(.36*std::cos(phase)),float(.36*std::sin(phase)),0);
        p.drawLine(project(motor+d,model),project(motor-d,model));
    }
    p.setPen(QColor("#17365D"));p.drawText(14,height()-38,QString("Roll %1°   Pitch %2°   Yaw %3°").arg(it->roll,0,'f',1).arg(it->pitch,0,'f',1).arg(it->yaw,0,'f',1));
    p.drawText(14,height()-14,QString("%1 • Görsel yükseklik ölçeklenmiştir • Gazebo ayrı pencerede").arg(armed?"ARMED":"DISARMED"));
}

void MapView::mouseDoubleClickEvent(QMouseEvent*e){if(!initialized_||!pointAdded)return;auto q=center_+e->position()-QPointF(width()/2.,height()/2.);double size=256.*std::pow(2,zoom_);double lon=q.x()/size*360-180,lat=std::atan(std::sinh(pi*(1-2*q.y()/size)))*180/pi;pointAdded(lat,lon);}
