#include "Scene3DView.hpp"
#include "ExperimentCore.hpp"
#include "MavlinkReadOnly.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMatrix4x4>
#include <QLinearGradient>
#include <QtMath>
#include <algorithm>
#include <cmath>
namespace {
constexpr double pi=3.141592653589793;
QColor hue(int id){if(id==1)return QColor("#EC4899");if(id==2)return QColor("#EF4444");return QColor("#FACC15");}
QColor deep(int id){if(id==1)return QColor("#9D174D");if(id==2)return QColor("#991B1B");return QColor("#854D0E");}
}
Scene3DView::Scene3DView(std::shared_ptr<FlightState>s,QWidget*p):QWidget(p),state_(std::move(s)){
    setObjectName("scene3DView");setMinimumSize(420,320);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
}
void Scene3DView::resetView(){azimuth_=38;elevation_=36;distance_=65;follow_=true;center_={};update();}
void Scene3DView::ensureOrigin(double now){
    if(originReady_)return;
    auto v=state_->vehicles.constFind(state_->selected);
    if(v!=state_->vehicles.cend()&&v->posAt>0&&now-v->posAt<3){originLat_=v->lat;originLon_=v->lon;originReady_=true;return;}
    for(const auto &item:state_->vehicles)if(item.posAt>0&&now-item.posAt<3){originLat_=item.lat;originLon_=item.lon;originReady_=true;return;}
}
QVector3D Scene3DView::toWorld(double lat,double lon,double alt)const{
    if(!originReady_)return {};
    auto m=aero::local(lat,lon,originLat_,originLon_);
    return {float(m.east),float(m.north),float(std::clamp(alt,0.,500.))};
}
void Scene3DView::paintEvent(QPaintEvent*){
    const double now=sitl::monoSeconds();ensureOrigin(now);
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::TextAntialiasing);
    QLinearGradient sky(0,0,0,height());sky.setColorAt(0,QColor("#DCEBFA"));sky.setColorAt(.55,QColor("#EFF6FF"));sky.setColorAt(1,QColor("#FFFFFF"));p.fillRect(rect(),sky);
    if(!originReady_){p.setPen(QColor("#17365D"));p.setFont(QFont("Sans Serif",14,QFont::DemiBold));p.drawText(rect(),Qt::AlignCenter,"3B GÖREV SAHNESİ\nDrone telemetrisi bekleniyor");return;}
    if(follow_){auto i=state_->vehicles.constFind(state_->selected);if(i!=state_->vehicles.cend()&&i->posAt>0&&now-i->posAt<3){auto v=toWorld(i->lat,i->lon,0);center_=QPointF(v.x(),v.y());}}
    double azi=azimuth_*pi/180,ele=elevation_*pi/180;
    QVector3D target(float(center_.x()),float(center_.y()),0);
    QVector3D eye=target+QVector3D(float(distance_*std::sin(azi)*std::cos(ele)),float(-distance_*std::cos(azi)*std::cos(ele)),float(distance_*std::sin(ele)+8));
    QMatrix4x4 view,projection;view.lookAt(eye,target+QVector3D(0,0,3),QVector3D(0,0,1));projection.perspective(48.,float(width())/std::max(1,height()),.1f,4000.f);
    const auto project=[&](QVector3D w){auto v=projection*view*QVector4D(w,1);if(v.w()<=.1f||!std::isfinite(v.w()))return QPointF(-10000,-10000);return QPointF((v.x()/v.w()+1)*width()*.5,(1-v.y()/v.w())*height()*.5);};
    const int lowX=int(std::floor(center_.x()/10)*10),lowY=int(std::floor(center_.y()/10)*10);
    // Bounded local reference surface and depth cues. No world coordinate spoofing.
    QVector3D corners[4]={{float(lowX-80),float(lowY-80),0},{float(lowX+80),float(lowY-80),0},{float(lowX+80),float(lowY+80),0},{float(lowX-80),float(lowY+80),0}};
    QPolygonF ground;for(auto c:corners)ground<<project(c);p.setPen(Qt::NoPen);p.setBrush(QColor(255,255,255,210));p.drawPolygon(ground);
    p.setPen(QPen(QColor("#C8D9ED"),1));for(int n=-80;n<=80;n+=10){p.drawLine(project({float(lowX+n),float(lowY-80),.01f}),project({float(lowX+n),float(lowY+80),.01f}));p.drawLine(project({float(lowX-80),float(lowY+n),.01f}),project({float(lowX+80),float(lowY+n),.01f}));}
    // East / north compass marks remain true to local ENU position coordinates.
    auto c=project({float(center_.x()),float(center_.y()),.02f});p.setPen(QPen(QColor("#4F76A5"),2));p.drawEllipse(c,5,5);
    if(plan_.size()>1){QPainterPath route;bool first=true;for(const auto &pt:plan_){auto r=toWorld(pt.y(),pt.x(),.15);auto q=project(r);if(first){route.moveTo(q);first=false;}else route.lineTo(q);p.setPen(QPen(QColor("#2563EB"),2));p.setBrush(QColor("#FFFFFF"));p.drawEllipse(q,4,4);}p.setPen(QPen(QColor("#2563EB"),2,Qt::DashLine));p.setBrush(Qt::NoBrush);p.drawPath(route);}
    hit_.clear();
    struct Glyph{int system;QVector3D pos;double yaw,pitch,roll;bool fresh;};QVector<Glyph> glyphs;
    for(auto it=state_->vehicles.cbegin();it!=state_->vehicles.cend();++it){
        const auto&v=it.value();if(v.posAt<=0)continue;
        QPainterPath trail;bool first=true;int start=std::max(0,int(v.trail.size())-600);
        for(int j=start;j<v.trail.size();++j){auto q=project(toWorld(v.trail[j].y(),v.trail[j].x(),.08));if(first){trail.moveTo(q);first=false;}else trail.lineTo(q);}
        p.setPen(QPen(hue(it.key()),it.key()==state_->selected?3:2,Qt::SolidLine,Qt::RoundCap));p.drawPath(trail);
        const auto pos=toWorld(v.lat,v.lon,v.alt);auto bottom=project({pos.x(),pos.y(),.1f});auto top=project(pos);
        bool live=v.heartbeatAt>0&&now-v.heartbeatAt<3&&now-v.posAt<3;
        p.setPen(QPen(QColor(23,54,93,90),1,Qt::DashLine));p.drawLine(bottom,top);p.setPen(Qt::NoPen);p.setBrush(QColor(47,93,160,45));p.drawEllipse(bottom,8,4);
        glyphs.append({it.key(),pos,v.yaw,v.pitch,v.roll,live});
    }
    std::sort(glyphs.begin(),glyphs.end(),[&](const Glyph&a,const Glyph&b){return (eye-a.pos).lengthSquared()>(eye-b.pos).lengthSquared();});
    for(const auto &g:glyphs){
        QMatrix4x4 model;model.translate(g.pos);model.rotate(float(-g.yaw),0,0,1);model.rotate(float(g.pitch),1,0,0);model.rotate(float(g.roll),0,1,0);
        const auto local=[&](float x,float y,float z=0){return project((model*QVector4D(x,y,z,1)).toVector3D());};
        auto mid=local(0,0);QColor paint=g.fresh?hue(g.system):QColor("#9CA3AF");p.setPen(QPen(deep(g.system),g.system==state_->selected?5:3,Qt::SolidLine,Qt::RoundCap));
        for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f})p.drawLine(mid,local(1.15f*sx,1.15f*sy));
        p.setPen(QPen(deep(g.system),2));p.setBrush(paint);
        p.drawPolygon(QPolygonF{local(-.45f,-.45f),local(.45f,-.45f),local(.45f,.4f),local(0,.75f,.12f),local(-.45f,.4f)});
        for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f}){auto q=local(1.15f*sx,1.15f*sy);p.setBrush(QColor(255,255,255,210));p.drawEllipse(q,7,7);p.setBrush(paint);p.drawEllipse(q,4,4);}
        if(g.system==state_->selected){p.setPen(QPen(paint,2));p.setBrush(Qt::NoBrush);p.drawEllipse(mid,30,30);}
        hit_.insert(g.system,mid);
        const QString label=QString(" SYS %1  %2 m %3").arg(g.system).arg(g.pos.z(),0,'f',1).arg(g.fresh?"":"• ESKİ");
        QFontMetrics fm(p.font());QRectF pill(QPointF(mid.x()+18,mid.y()-29),QSizeF(fm.horizontalAdvance(label)+17,25));p.setBrush(QColor(255,255,255,245));p.setPen(QPen(QColor("#C8D9ED"),1));p.drawRoundedRect(pill,11,11);p.setPen(deep(g.system));p.drawText(pill.adjusted(8,0,-5,0),Qt::AlignVCenter,label);
    }
    QRectF header(15,14,std::min(width()-30,510),59);p.setPen(Qt::NoPen);p.setBrush(QColor(255,255,255,242));p.drawRoundedRect(header,15,15);
    p.setFont(QFont("Sans Serif",12,QFont::DemiBold));p.setPen(QColor("#17365D"));p.drawText(header.adjusted(16,8,-8,-27),Qt::AlignVCenter,"AEROCORE  /  3B GÖREV SAHNESİ");
    p.setFont(QFont("Sans Serif",9));p.setPen(QColor("#1E4E82"));p.drawText(header.adjusted(16,35,-8,0),Qt::AlignVCenter,"Canlı telemetri • grid temsilidir • sürükle: döndür • tekerlek: yakınlaş");
    p.setPen(QPen(QColor("#1E4E82"),1));p.setBrush(QColor(255,255,255,225));p.drawRoundedRect(QRectF(width()-178,height()-39,163,25),9,9);p.drawText(QRectF(width()-170,height()-36,150,20),Qt::AlignCenter,QString("SYS %1  •  %2").arg(state_->selected).arg(follow_?"TAKİP":"SERBEST"));
}
void Scene3DView::wheelEvent(QWheelEvent*e){distance_=std::clamp(distance_*std::pow(.88,e->angleDelta().y()/120.),12.,500.);update();e->accept();}
void Scene3DView::mousePressEvent(QMouseEvent*e){lastMouse_=e->position();dragged_=false;e->accept();}
void Scene3DView::mouseMoveEvent(QMouseEvent*e){if(!(e->buttons()&Qt::LeftButton))return;auto delta=e->position()-lastMouse_;if(delta.manhattanLength()>2)dragged_=true;lastMouse_=e->position();if(e->modifiers()&Qt::ShiftModifier){center_.rx()-=delta.x()*distance_/std::max(100,width());center_.ry()+=delta.y()*distance_/std::max(100,height());follow_=false;}else {azimuth_=std::fmod(azimuth_+delta.x()*.35+3600.,360.);elevation_=std::clamp(elevation_+delta.y()*.25,12.,78.);}update();e->accept();}
void Scene3DView::mouseReleaseEvent(QMouseEvent*e){if(!dragged_){int best=0;double dist=625;for(auto it=hit_.begin();it!=hit_.end();++it){double d=QPointF(it.value()-e->position()).manhattanLength();d*=d*.5;if(d<dist){best=it.key();dist=d;}}if(best&&(!selected||selected(best))){state_->selected=best;follow_=true;update();}}e->accept();}
